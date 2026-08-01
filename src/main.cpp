#include "UI/Panel.h"
#include "Core/Log.h"
#include "Core/InlineExpander.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>

// ============================================================================
// Global state
// ============================================================================
static std::atomic<bool> g_toggleRequested{false};
static std::atomic<bool> g_panelVisible{false};
static GLFWwindow* g_window = nullptr;
static ssp::Panel* g_panel = nullptr;

#ifdef _WIN32
static HWND g_hwndPrevFocus = nullptr;  // Window that had focus before panel appeared
static HWND g_sspHwnd = nullptr;
static bool g_inserting = false;
#endif
#ifdef _WIN32
static ssp::InlineExpander g_expander;
#endif

// ============================================================================
// Windows: message-only window for global hotkey (Alt+A)
// ============================================================================
#ifdef _WIN32
static HWND g_hotkeyHwnd = nullptr;
static constexpr int HOTKEY_ID = 1;

static LRESULT CALLBACK HotkeyWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_HOTKEY && wp == HOTKEY_ID) {
        g_toggleRequested = true;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void HotkeyThread() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HotkeyWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SSP_HotkeyWindow";
    RegisterClassExW(&wc);

    g_hotkeyHwnd = CreateWindowExW(0, L"SSP_HotkeyWindow", L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hotkeyHwnd) return;

    if (!RegisterHotKey(g_hotkeyHwnd, HOTKEY_ID, MOD_ALT, 'A')) {
        RegisterHotKey(g_hotkeyHwnd, HOTKEY_ID, MOD_ALT, VK_SPACE);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void UnregisterHotkey() {
    if (g_hotkeyHwnd) {
        UnregisterHotKey(g_hotkeyHwnd, HOTKEY_ID);
        DestroyWindow(g_hotkeyHwnd);
        g_hotkeyHwnd = nullptr;
    }
}
#endif

// ============================================================================
// Platform: type Unicode text into a specific window
// ============================================================================
#ifdef _WIN32
static void TypeTextIntoWindow(HWND target, const std::wstring& text) {
    if (!target || !IsWindow(target)) return;

    // Give focus to the target window
    SetForegroundWindow(target);
    Sleep(15); // Let the window manager settle

    // Send Unicode characters via SendInput
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
#endif

// ============================================================================
// Center window on the monitor containing the cursor
// ============================================================================
static void CenterWindow(GLFWwindow* window) {
#ifdef _WIN32
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR hmon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hmon, &mi);
    RECT work = mi.rcWork;
    int ww, wh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwSetWindowPos(window,
        ((work.right - work.left) - ww) / 2 + work.left,
        ((work.bottom - work.top) - wh) / 2 + work.top);
#endif
}

// ============================================================================
// Show panel — steal focus, but remember who had it
// ============================================================================
static void ShowPanel() {
#ifdef _WIN32
    g_hwndPrevFocus = GetForegroundWindow();
    CenterWindow(g_window);
    glfwShowWindow(g_window);
    glfwFocusWindow(g_window);
    g_panelVisible = true;
    g_panel->OnShow();
    g_expander.SetPanelVisible(true);
#endif
}

// ============================================================================
// Hide panel — restore focus to the previous window
// ============================================================================
static void HidePanel() {
#ifdef _WIN32
    g_panelVisible = false;
    g_panel->OnHide();
    g_expander.SetPanelVisible(false);
    glfwHideWindow(g_window);

    // Restore focus to the window that had it before the panel appeared
    if (g_hwndPrevFocus && IsWindow(g_hwndPrevFocus)) {
        SetForegroundWindow(g_hwndPrevFocus);
    }
#endif
}

static void InsertTextAndKeepPanelOpen(const std::wstring& text) {
#ifdef _WIN32
    HWND target = g_hwndPrevFocus;
    if (!target || !IsWindow(target) || target == g_sspHwnd) return;

    g_inserting = true;
    glfwHideWindow(g_window);
    TypeTextIntoWindow(target, text);
    Sleep(25);
    glfwShowWindow(g_window);
    SetForegroundWindow(g_sspHwnd);
    glfwFocusWindow(g_window);
    g_inserting = false;
#else
    (void)text;
#endif
}

// Insert is handled directly in the main loop after rendering.

static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// ============================================================================
// GLFW focus callback — hide panel when focus is lost (clicked outside)
// ============================================================================
static void FocusCallback(GLFWwindow* window, int focused) {
    (void)window;
#ifdef _WIN32
    if (!focused && g_panelVisible && !g_inserting) {
        HidePanel();
    }
#endif
}

// ============================================================================
// Main
// ============================================================================

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);         // Start hidden
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);       // No title bar / X button
    glfwWindowHint(GLFW_FLOATING, GL_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GL_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GL_FALSE);         // Don't grab focus on creation

    g_window = glfwCreateWindow(380, 720, "Scientific Symbol Panel", nullptr, nullptr);
    if (!g_window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    // Track focus loss (click outside = hide)
    glfwSetWindowFocusCallback(g_window, FocusCallback);

#ifdef _WIN32
    g_sspHwnd = glfwGetWin32Window(g_window);
#endif

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Font loading
    ImFontGlyphRangesBuilder symBuilder;
    symBuilder.AddRanges(io.Fonts->GetGlyphRangesGreek());
    static const ImWchar symbolRanges[] = {
        0x00A0, 0x00FF, 0x0370, 0x03FF, 0x2000, 0x206F,
        0x2070, 0x209F, 0x20A0, 0x20CF, 0x2100, 0x214F,
        0x2150, 0x218F, 0x2190, 0x21FF, 0x2200, 0x22FF,
        0x2300, 0x23FF, 0x25A0, 0x25FF, 0x2600, 0x26FF,
        0x2700, 0x27BF, 0x27C0, 0x27EF, 0x2980, 0x29FF,
        0x2A00, 0x2AFF, 0xFE50, 0xFE6F, 0,
    };
    symBuilder.AddRanges(symbolRanges);
    ImVector<ImWchar> symRanges;
    symBuilder.BuildRanges(&symRanges);

#ifdef _WIN32
    ImFontConfig baseCfg; baseCfg.SizePixels = 27.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 27.0f, &baseCfg, io.Fonts->GetGlyphRangesDefault());
    ImFontConfig symCfg; symCfg.SizePixels = 27.0f; symCfg.MergeMode = true; symCfg.GlyphMinAdvanceX = 27.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 27.0f, &symCfg, symRanges.Data);
#else
    const char* fonts[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr
    };
    bool loaded = false;
    for (int i = 0; fonts[i]; i++) {
        FILE* f = fopen(fonts[i], "rb");
        if (f) { fclose(f); io.Fonts->AddFontFromFileTTF(fonts[i], 27.0f, nullptr, symRanges.Data); loaded = true; break; }
    }
    if (!loaded) io.Fonts->AddFontDefault(nullptr);
#endif

    // Init panel
    ssp::Panel panel;
    g_panel = &panel;
    if (!panel.Initialize(g_window)) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }


    // Global hotkey (Windows)
#ifdef _WIN32
    std::thread hotkeyThread(HotkeyThread);
    hotkeyThread.detach();
#endif

    // Inline text expansion (system-wide, runtime-configurable)
#ifdef _WIN32
    if (g_expander.Initialize(panel.GetConfig().Get().inlineExpander)) {
        g_expander.Start();
    }
#endif
    // Insert dance state — done between frames to avoid corrupting ImGui
    // Main loop
    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents();

        // Hotkey toggle
        if (g_toggleRequested.exchange(false)) {
            if (g_panelVisible) HidePanel();
            else ShowPanel();
        }

        // Panel wants to hide (Escape)
        if (g_panelVisible && panel.WantsHide()) {
            HidePanel();
        }

        // Render
        if (g_panelVisible) {
            panel.Render();
            glfwSwapBuffers(g_window);

#ifdef _WIN32
            // Insert after the frame is done so focus changes do not corrupt ImGui state.
            if (panel.HasPendingInsert()) {
                std::wstring text = panel.TakePendingInsert();
                InsertTextAndKeepPanelOpen(text);
            }
#endif
        } else {
            glfwWaitEventsTimeout(0.1);
        }
    }

    // Cleanup
    panel.Shutdown();
#ifdef _WIN32
    UnregisterHotkey();
#endif
#ifdef _WIN32
    g_expander.Shutdown();
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_window);
    glfwTerminate();

    return 0;
}

#ifdef _WIN32
#include <windows.h>
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    return main();
}
#endif
