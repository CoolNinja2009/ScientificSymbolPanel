#pragma once

#include "Core/Types.h"

#ifdef _WIN32

#include <windows.h>
#include <atomic>
#include <string>
#include <memory>
#include <thread>

namespace ssp {

class SearchEngine;
class SymbolDatabase;

// ============================================================================
// InlineExpander — system-wide text expansion via WH_KEYBOARD_LL hook
//
// Watches keystrokes globally. When the user types <word><trigger><space> in
// any textbox, it deletes the word + trigger and inserts the best-matching
// Unicode symbol. Trigger, case preference, and custom mappings are
// configurable at runtime via InlineExpanderSettings (no recompile).
// ============================================================================
class InlineExpander {
public:
    InlineExpander();
    ~InlineExpander();

    // Non-copyable
    InlineExpander(const InlineExpander&) = delete;
    InlineExpander& operator=(const InlineExpander&) = delete;

    // Load database + build search index. Call from main thread before Start().
    bool Initialize(const InlineExpanderSettings& settings);

    // Start the keyboard hook thread.
    void Start();

    // Stop hook thread, uninstall hook, join.
    void Stop();

    // Shorthand: Stop + cleanup.
    void Shutdown();

    // Panel-visible flag — set by main thread to suppress expansion while
    // the user is typing inside the SSP panel itself.
    void SetPanelVisible(bool visible) { m_panelVisible = visible; }

private:
    // Hook proc — called on the hook thread via Windows message pump.
    static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // Thread entry: install hook, pump messages.
    void ThreadProc();

    // Append a character to the keystroke buffer.
    void AppendToBuffer(wchar_t ch);

    // Check if buffer ends with <word><trigger><space>; if so, expand.
    // Returns true when the triggering space should be consumed.
    bool CheckTrigger();

    // Do the actual deletion + replacement via SendInput.
    void DoReplacement(const std::wstring& word);

    // Check if m_trigger matches at position pos in m_buffer.
    bool MatchTriggerAt(size_t pos) const;

    // --- State ---
    std::unique_ptr<SymbolDatabase> m_database;
    std::unique_ptr<SearchEngine>   m_searchEngine;

    HHOOK     m_hook = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_panelVisible{false};

    // Config (read at startup from settings.json)
    std::wstring m_trigger;
    bool m_preferLowercase = true;
    std::unordered_map<std::wstring, int32_t> m_customMappings;

    // Keystroke buffer — last ~256 chars typed globally.
    std::wstring m_buffer;

    static InlineExpander* s_instance;  // singleton for HookProc callback
};

} // namespace ssp

#endif // _WIN32
