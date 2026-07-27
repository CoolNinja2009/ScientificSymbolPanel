#pragma once
// Wrapper around stb_textedit for wstring-based single-line text editing.
// Used by InputHandler for the search bar.

#include <string>
#include <string_view>
#include <cwctype>


namespace ssp {

// Opaque state — STB_TexteditState is defined in the implementation.
struct TextEditState;

// Factory — creates and initializes a TextEditState
TextEditState* TextEdit_Create();
void TextEdit_Destroy(TextEditState* state);

// Initializes state for single-line editing. Call once on construction.
void TextEdit_Init(TextEditState* state);

// Sets the underlying string this editor operates on.
void TextEdit_SetString(TextEditState* state, std::wstring* str);

// --- Mouse ---
// x is horizontal pixel offset from left edge of text area.
// y is ignored (single-line).
void TextEdit_Click(TextEditState* state, float x);
void TextEdit_Drag(TextEditState* state, float x);

// --- Keyboard ---
// key is a VK_* virtual key code (e.g. VK_LEFT, VK_BACK, VK_DELETE).
// ctrl/shift are modifier flags.
// Returns true if the key was consumed.
bool TextEdit_Key(TextEditState* state, int vk, bool ctrl, bool shift);

// --- Character input ---
// Inserts a printable character at the cursor (or over selection).
void TextEdit_Char(TextEditState* state, wchar_t ch);

// --- Paste ---
void TextEdit_Paste(TextEditState* state, const std::wstring& text);

// --- Queries ---
int  TextEdit_GetCursor(const TextEditState* state);
bool TextEdit_HasSelection(const TextEditState* state);
int  TextEdit_GetSelectStart(const TextEditState* state);
int  TextEdit_GetSelectEnd(const TextEditState* state);

// --- Cut/Copy support ---
// Deletes selection; caller should copy selection text to clipboard first.
bool TextEdit_Cut(TextEditState* state);
// Returns the currently selected text (empty if no selection).
std::wstring TextEdit_GetSelection(const TextEditState* state);

} // namespace ssp
