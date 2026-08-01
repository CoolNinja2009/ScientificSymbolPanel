#ifdef _WIN32

#include "Core/InlineExpander.h"
#include "Symbols/Database.h"
#include "Search/SearchEngine.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace ssp {

InlineExpander* InlineExpander::s_instance = nullptr;

// ============================================================================
// Construction / Destruction
// ============================================================================

InlineExpander::InlineExpander() = default;

InlineExpander::~InlineExpander() {
    Shutdown();
}

// ============================================================================
// Initialize — load database, build search engine, store config
// ============================================================================

bool InlineExpander::Initialize(const InlineExpanderSettings& settings) {
    m_trigger          = settings.trigger;
    m_preferLowercase  = settings.preferLowercase;
    m_customMappings   = settings.customMappings;  // copy — hook thread reads this

    m_database = std::make_unique<SymbolDatabase>();
    if (!m_database->Load()) return false;

    m_searchEngine = std::make_unique<SearchEngine>();
    m_searchEngine->Build(m_database->GetSymbols());

    return true;
}

// ============================================================================
// Start / Stop
// ============================================================================

void InlineExpander::Start() {
    if (m_running) return;
    m_running = true;
    s_instance = this;
    m_thread = std::thread(&InlineExpander::ThreadProc, this);
}

void InlineExpander::Stop() {
    if (!m_running) return;

    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }

    m_running = false;
    PostThreadMessageW(GetThreadId(m_thread.native_handle()), WM_QUIT, 0, 0);

    if (m_thread.joinable())
        m_thread.join();

    s_instance = nullptr;
}

void InlineExpander::Shutdown() {
    Stop();
    m_searchEngine.reset();
    m_database.reset();
}

// ============================================================================
// Thread proc — install hook, pump messages
// ============================================================================

void InlineExpander::ThreadProc() {
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc,
                                GetModuleHandleW(nullptr), 0);
    if (!m_hook) {
        m_running = false;
        return;
    }

    MSG msg;
    while (m_running && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
}

// ============================================================================
// Hook proc — called on hook thread for every system keystroke
// ============================================================================

LRESULT CALLBACK InlineExpander::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !s_instance)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    if (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    auto& self = *s_instance;

    if (self.m_panelVisible)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    DWORD vk = kb->vkCode;

    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool caps  = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    bool isAlpha = (vk >= 'A' && vk <= 'Z');
    bool lower = isAlpha && (!shift != !caps);

    if (isAlpha) {
        wchar_t ch = lower ? static_cast<wchar_t>(vk - 'A' + 'a')
                           : static_cast<wchar_t>(vk);
        self.AppendToBuffer(ch);
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // Backtick / grave accent: always buffer as ` or ~
    if (vk == VK_OEM_3) {
        wchar_t ch = shift ? L'~' : L'`';
        self.AppendToBuffer(ch);
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // Space: buffers, then checks for trigger
    if (vk == VK_SPACE) {
        self.AppendToBuffer(L' ');
        if (self.CheckTrigger())
            return 1;  // consume the space
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (vk == VK_BACK) {
        if (!self.m_buffer.empty())
            self.m_buffer.pop_back();
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // Navigation / commit keys → clear buffer
    if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_TAB ||
        vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) {
        self.m_buffer.clear();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ============================================================================
// Buffer
// ============================================================================

void InlineExpander::AppendToBuffer(wchar_t ch) {
    if (m_buffer.size() >= 256)
        m_buffer.erase(0, m_buffer.size() - 200);
    m_buffer.push_back(ch);
}

// ============================================================================
// Trigger detection — configurable trigger string
// ============================================================================

bool InlineExpander::MatchTriggerAt(size_t pos) const {
    if (pos + m_trigger.size() > m_buffer.size()) return false;
    for (size_t i = 0; i < m_trigger.size(); i++) {
        if (m_buffer[pos + i] != m_trigger[i]) return false;
    }
    return true;
}

bool InlineExpander::CheckTrigger() {
    size_t tlen = m_trigger.size();
    size_t minLen = 1 + tlen + 1;  // word(1+) + trigger + space
    if (m_buffer.size() < minLen) return false;

    size_t len = m_buffer.length();

    // Pattern: … word <trigger> <space>
    if (m_buffer[len - 1] != L' ') return false;

    // Check trigger before the space
    if (!MatchTriggerAt(len - 1 - tlen)) return false;

    // Extract word before trigger
    size_t wordStart = len - 1 - tlen;
    while (wordStart > 0 && std::iswalpha(static_cast<wint_t>(m_buffer[wordStart - 1]))) {
        wordStart--;
    }

    size_t wordLen = len - 1 - tlen - wordStart;
    if (wordLen == 0) {
        m_buffer.clear();
        return true;  // consume the space even with no word
    }

    std::wstring word = m_buffer.substr(wordStart, wordLen);
    m_buffer.clear();
    DoReplacement(word);
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

static bool IsLowercaseGreek(const Symbol* sym) {
    if (!sym || sym->symbol.empty()) return false;
    wchar_t ch = sym->symbol[0];
    return (ch >= 0x03B1 && ch <= 0x03C9);
}

static void SendBackspaces(int count) {
    if (count <= 0) return;
    std::vector<INPUT> inputs;
    inputs.reserve(static_cast<size_t>(count) * 2);
    for (int i = 0; i < count; i++) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

static void SendUnicodeText(const std::wstring& text) {
    if (text.empty()) return;
    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);
    for (wchar_t ch : text) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

// ============================================================================
// Replacement
// ============================================================================

void InlineExpander::DoReplacement(const std::wstring& word) {
    const Symbol* best = nullptr;

    // 1. Check custom mappings first (keyword → symbol index)
    auto it = m_customMappings.find(word);
    if (it != m_customMappings.end() && m_database) {
        int32_t idx = it->second;
        const auto& symbols = m_database->GetSymbols();
        if (idx >= 0 && static_cast<size_t>(idx) < symbols.size()) {
            best = &symbols[static_cast<size_t>(idx)];
        }
    }

    // 2. Fall back to search engine
    if (!best && m_searchEngine) {
        auto results = m_searchEngine->Search(word);
        if (!results.empty()) {
            best = results[0].symbol;
            int bestScore = results[0].score;

            // Prefer lowercase Greek among equally-scored results
            if (m_preferLowercase) {
                for (const auto& r : results) {
                    if (r.score < bestScore) break;
                    if (r.symbol && IsLowercaseGreek(r.symbol)) {
                        best = r.symbol;
                        break;
                    }
                }
            }
        }
    }

    if (!best || best->symbol.empty()) return;

    // Delete: word + trigger (space is consumed by the hook)
    int deleteCount = static_cast<int>(word.length() + m_trigger.size());

    SendBackspaces(deleteCount);
    Sleep(10);
    SendUnicodeText(best->symbol);
}

} // namespace ssp

#endif // _WIN32
