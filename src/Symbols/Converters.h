#pragma once
#include <string>
#include <string_view>

namespace ssp {

// ============================================================================
// ScientificConverter — "6.02e23" → "6.02 × 10²³"
// ============================================================================
class ScientificConverter {
public:
    // Parses scientific notation: [0-9.]+[eE][+-]?[0-9]+
    // Returns empty string on no match.
    static std::wstring Convert(std::wstring_view input);
};

// ============================================================================
// SuperscriptBuilder — "x^2" → "x²", "x^23" → "x²³"
// ============================================================================
class SuperscriptBuilder {
public:
    // Converts ^<chars> patterns to superscript Unicode.
    // Returns input unchanged if no patterns found.
    static std::wstring Convert(std::wstring_view input);
};

// ============================================================================
// SubscriptBuilder — "CO2" → "CO₂", "H2SO4" → "H₂SO₄"
// ============================================================================
class SubscriptBuilder {
public:
    // Converts letter-followed-by-digit sequences to subscript Unicode.
    // Only converts digits immediately following a letter; standalone numbers
    // and numbers after spaces are left alone.
    // Returns input unchanged if no patterns found.
    static std::wstring Convert(std::wstring_view input);
};

// ============================================================================
// FractionBuilder — "1/2" → "½"
// ============================================================================
class FractionBuilder {
public:
    // Converts common ASCII fractions to their Unicode single-codepoint form.
    // Returns input unchanged if no known fraction found.
    static std::wstring Convert(std::wstring_view input);
};

// ============================================================================
// LaTeXConverter — "\\pi" → "π", "\\rightarrow" → "→"
// ============================================================================
class LaTeXConverter {
public:
    // Converts LaTeX command aliases to Unicode glyphs.
    // Case-insensitive match for \\command.
    // Returns input unchanged if no match found.
    static std::wstring Convert(std::wstring_view input);
};

} // namespace ssp
