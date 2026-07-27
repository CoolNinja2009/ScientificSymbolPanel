#include <string>
#include <cwctype>
#include <windows.h>

// stb_textedit integration for wstring-based single-line text editing.
#define STB_TEXTEDIT_CHARTYPE   wchar_t
#define STB_TEXTEDIT_POSITIONTYPE int

// Our string wrapper — just a pointer to the wstring being edited.
struct TextEditString {
    std::wstring* str;
};

#define STB_TEXTEDIT_STRING         TextEditString
#define STB_TEXTEDIT_STRINGLEN(obj) ((int)(obj)->str->size())
#define STB_TEXTEDIT_GETCHAR(obj,i) ((obj)->str->at(i))
#define STB_TEXTEDIT_DELETECHARS(obj,i,n) \
    do { (obj)->str->erase(i, n); } while(0)
#define STB_TEXTEDIT_INSERTCHARS(obj,i,chars,n) \
    ((obj)->str->insert(i, chars, n), true)
#define STB_TEXTEDIT_NEWLINE        L'\n'
// Key-to-text: we only use this for printable chars (handled separately via TextEdit_Char),
// so return -1 to indicate "not an insertable character from key events".
#define STB_TEXTEDIT_KEYTOTEXT(k)  (-1)

// Key definitions — we map VK_* codes to these.
// We use the high bit (0x80000000) as a "keydown" flag since our
// VK codes are small positive ints and can't have that bit set natively.
#define STB_TEXTEDIT_KEYDOWN_BIT    0x40000000

#define STB_TEXTEDIT_CTRL_BIT       0x10000000

#define STB_TEXTEDIT_K_LEFT         (VK_LEFT   | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_RIGHT        (VK_RIGHT  | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_UP           (VK_UP     | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_DOWN         (VK_DOWN   | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_PGUP         (VK_PRIOR  | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_PGDOWN       (VK_NEXT   | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_LINESTART    (VK_HOME   | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_LINEEND      (VK_END    | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_TEXTSTART    (VK_HOME   | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_TEXTEND      (VK_END    | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_DELETE       (VK_DELETE | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_BACKSPACE    (VK_BACK   | STB_TEXTEDIT_KEYDOWN_BIT)
#define STB_TEXTEDIT_K_UNDO         ('Z'       | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_REDO         ('Y'       | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_WORDLEFT     (VK_LEFT   | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_WORDRIGHT    (VK_RIGHT  | STB_TEXTEDIT_KEYDOWN_BIT | STB_TEXTEDIT_CTRL_BIT)
#define STB_TEXTEDIT_K_SHIFT        0x20000000

// Single-line layout: one row containing all characters
// Approximate char width — we use a fixed estimate since we're single-line
#define STB_TEXTEDIT_GETWIDTH(obj,n,i)  (8.5f)

#define STB_TEXTEDIT_LAYOUTROW(r,obj,n) \
    do { \
        (r)->x0 = 0; \
        (r)->x1 = (float)STB_TEXTEDIT_STRINGLEN(obj) * 8.5f; \
        (r)->baseline_y_delta = 20.0f; \
        (r)->ymin = -2.0f; \
        (r)->ymax = 18.0f; \
        (r)->num_chars = STB_TEXTEDIT_STRINGLEN(obj) - (n); \
    } while(0)

#define STB_TEXTEDIT_IS_SPACE(ch)   (std::iswspace(ch))

// Map VK code to stb_textedit key with modifiers
static int MapKey(int vk, bool ctrl, bool shift) {
    int k = vk | STB_TEXTEDIT_KEYDOWN_BIT;
    if (shift) k |= STB_TEXTEDIT_K_SHIFT;
    // For Ctrl+Left/Right, stb uses STB_TEXTEDIT_K_WORDLEFT/WORDRIGHT
    // which we've defined as the same VK codes — stb_textedit checks
    // for K_WORDLEFT before K_LEFT in its handler, so we prepend a ctrl flag.
    // Actually, stb_textedit doesn't have a ctrl concept — it relies on
    // the caller to map Ctrl+Left -> K_WORDLEFT. We do this by returning
    // the word-move key when ctrl is pressed.
    if (ctrl) {
        if (vk == VK_LEFT)  return STB_TEXTEDIT_K_WORDLEFT  | (shift ? STB_TEXTEDIT_K_SHIFT : 0);
        if (vk == VK_RIGHT) return STB_TEXTEDIT_K_WORDRIGHT | (shift ? STB_TEXTEDIT_K_SHIFT : 0);
        if (vk == VK_BACK)  return STB_TEXTEDIT_K_WORDLEFT  | (shift ? STB_TEXTEDIT_K_SHIFT : 0); // Ctrl+Backspace = delete word left
        if (vk == VK_DELETE)return STB_TEXTEDIT_K_WORDRIGHT | (shift ? STB_TEXTEDIT_K_SHIFT : 0); // Ctrl+Delete = delete word right
        if (vk == 'Z')      return STB_TEXTEDIT_K_UNDO;
        if (vk == 'Y')      return STB_TEXTEDIT_K_REDO;
        if (vk == 'A')      return 'A' | STB_TEXTEDIT_KEYDOWN_BIT; // handled separately
        if (vk == 'C')      return 'C' | STB_TEXTEDIT_KEYDOWN_BIT;
        if (vk == 'V')      return 'V' | STB_TEXTEDIT_KEYDOWN_BIT;
        if (vk == 'X')      return 'X' | STB_TEXTEDIT_KEYDOWN_BIT;
    }
    return k;
}

// Include the implementation
#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

// ============================================================================
// Public API — thin wrappers around stb_textedit
// ============================================================================

namespace ssp {

struct TextEditState {
    STB_TexteditState stb;
    TextEditString strWrapper;
};

void TextEdit_Init(TextEditState* s) {
    stb_textedit_initialize_state(&s->stb, 1); // single_line = true
}

void TextEdit_SetString(TextEditState* s, std::wstring* str) {
    s->strWrapper.str = str;
}

void TextEdit_Click(TextEditState* s, float x) {
    stb_textedit_click(&s->strWrapper, &s->stb, x, 0);
}

void TextEdit_Drag(TextEditState* s, float x) {
    stb_textedit_drag(&s->strWrapper, &s->stb, x, 0);
}

bool TextEdit_Key(TextEditState* s, int vk, bool ctrl, bool shift) {
    int key = MapKey(vk, ctrl, shift);
    stb_textedit_key(&s->strWrapper, &s->stb, key);
    return true;
}

void TextEdit_Char(TextEditState* s, wchar_t ch) {
    // Insert character at cursor (stb_textedit handles selection replacement)
    wchar_t chars[2] = { ch, L'\0' };
    stb_textedit_paste(&s->strWrapper, &s->stb, chars, 1);
}

void TextEdit_Paste(TextEditState* s, const std::wstring& text) {
    stb_textedit_paste(&s->strWrapper, &s->stb, text.data(), (int)text.size());
}

int TextEdit_GetCursor(const TextEditState* s) { return s->stb.cursor; }
bool TextEdit_HasSelection(const TextEditState* s) { return s->stb.select_start != s->stb.select_end; }
int TextEdit_GetSelectStart(const TextEditState* s) { return s->stb.select_start; }
int TextEdit_GetSelectEnd(const TextEditState* s) { return s->stb.select_end; }

bool TextEdit_Cut(TextEditState* s) {
    return stb_textedit_cut(&s->strWrapper, &s->stb) != 0;
}

std::wstring TextEdit_GetSelection(const TextEditState* s) {
    if (!TextEdit_HasSelection(s)) return {};
    int lo = s->stb.select_start < s->stb.select_end ? s->stb.select_start : s->stb.select_end;
    int hi = s->stb.select_start < s->stb.select_end ? s->stb.select_end : s->stb.select_start;
    return s->strWrapper.str->substr(lo, hi - lo);
}

} // namespace ssp

namespace ssp {

TextEditState* TextEdit_Create() {
    auto* s = new TextEditState();
    TextEdit_Init(s);
    return s;
}

void TextEdit_Destroy(TextEditState* state) {
    delete state;
}

} // namespace ssp
