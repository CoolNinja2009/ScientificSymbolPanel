#include "Converters.h"
#include "Core/Log.h"
#include <unordered_map>
#include <cwctype>

namespace ssp {

// ============================================================================
// Shared helpers
// ============================================================================

namespace {

wchar_t ToSuperscriptDigit(int d) {
    static constexpr wchar_t kSuperscriptDigits[] = {
        L'\x2070', // ⁰
        L'\x00B9', // ¹
        L'\x00B2', // ²
        L'\x00B3', // ³
        L'\x2074', // ⁴
        L'\x2075', // ⁵
        L'\x2076', // ⁶
        L'\x2077', // ⁷
        L'\x2078', // ⁸
        L'\x2079', // ⁹
    };
    return (d >= 0 && d <= 9) ? kSuperscriptDigits[d] : L'?';
}

wchar_t ToSubscriptDigit(int d) {
    static constexpr wchar_t kSubscriptDigits[] = {
        L'\x2080', // ₀
        L'\x2081', // ₁
        L'\x2082', // ₂
        L'\x2083', // ₃
        L'\x2084', // ₄
        L'\x2085', // ₅
        L'\x2086', // ₆
        L'\x2087', // ₇
        L'\x2088', // ₈
        L'\x2089', // ₉
    };
    return (d >= 0 && d <= 9) ? kSubscriptDigits[d] : L'?';
}

wchar_t ToSuperscriptChar(wchar_t ch) {
    switch (ch) {
    case L'0': return L'\x2070'; // ⁰
    case L'1': return L'\x00B9'; // ¹
    case L'2': return L'\x00B2'; // ²
    case L'3': return L'\x00B3'; // ³
    case L'4': return L'\x2074'; // ⁴
    case L'5': return L'\x2075'; // ⁵
    case L'6': return L'\x2076'; // ⁶
    case L'7': return L'\x2077'; // ⁷
    case L'8': return L'\x2078'; // ⁸
    case L'9': return L'\x2079'; // ⁹
    case L'+': return L'\x207A'; // ⁺
    case L'-': return L'\x207B'; // ⁻
    case L'=': return L'\x207C'; // ⁼
    case L'(': return L'\x207D'; // ⁽
    case L')': return L'\x207E'; // ⁾
    case L'a': case L'A': return L'\x1D43'; // ᵃ
    case L'b': case L'B': return L'\x1D47'; // ᵇ
    case L'c': case L'C': return L'\x1D9C'; // ᶜ
    case L'd': case L'D': return L'\x1D48'; // ᵈ
    case L'e': case L'E': return L'\x1D49'; // ᵉ
    case L'f': case L'F': return L'\x1DA0'; // ᶠ
    case L'g': case L'G': return L'\x1D4D'; // ᵍ
    case L'h': case L'H': return L'\x02B0'; // ʰ
    case L'i': case L'I': return L'\x2071'; // ⁱ
    case L'j': case L'J': return L'\x02B2'; // ʲ
    case L'k': case L'K': return L'\x1D4F'; // ᵏ
    case L'l': case L'L': return L'\x02E1'; // ˡ
    case L'm': case L'M': return L'\x1D50'; // ᵐ
    case L'n': case L'N': return L'\x207F'; // ⁿ
    case L'o': case L'O': return L'\x1D52'; // ᵒ
    case L'p': case L'P': return L'\x1D56'; // ᵖ
    case L'r': case L'R': return L'\x02B3'; // ʳ
    case L's': case L'S': return L'\x02E2'; // ˢ
    case L't': case L'T': return L'\x1D57'; // ᵗ
    case L'u': case L'U': return L'\x1D58'; // ᵘ
    case L'v': case L'V': return L'\x1D5B'; // ᵛ
    case L'w': case L'W': return L'\x02B7'; // ʷ
    case L'x': case L'X': return L'\x02E3'; // ˣ
    case L'y': case L'Y': return L'\x02B8'; // ʸ
    case L'z': case L'Z': return L'\x1DBB'; // ᶻ
    default: return L'\0';
    }
}

bool HasSuperscriptForm(wchar_t ch) {
    return ToSuperscriptChar(ch) != L'\0';
}

bool IsAsciiLetter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

bool IsAsciiDigit(wchar_t ch) {
    return ch >= L'0' && ch <= L'9';
}

} // anonymous namespace

// ============================================================================
// ScientificConverter
// ============================================================================

std::wstring ScientificConverter::Convert(std::wstring_view input) {
    // Match [0-9.]+[eE][+-]?[0-9]+
    size_t ePos = std::wstring_view::npos;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == L'e' || input[i] == L'E') {
            ePos = i;
            break;
        }
    }
    if (ePos == std::wstring_view::npos || ePos == 0) return {};

    auto mantissa = input.substr(0, ePos);
    bool hasDigit = false;
    for (auto ch : mantissa) {
        if (ch >= L'0' && ch <= L'9') { hasDigit = true; break; }
    }
    if (!hasDigit) return {};

    for (auto ch : mantissa) {
        if (ch != L'.' && !(ch >= L'0' && ch <= L'9')) return {};
    }

    size_t expStart = ePos + 1;
    if (expStart >= input.size()) return {};

    bool negative = false;
    if (input[expStart] == L'-') {
        negative = true;
        ++expStart;
    } else if (input[expStart] == L'+') {
        ++expStart;
    }

    if (expStart >= input.size()) return {};

    auto expStr = input.substr(expStart);
    if (expStr.empty()) return {};

    int exponent = 0;
    for (auto ch : expStr) {
        if (ch < L'0' || ch > L'9') return {};
        exponent = exponent * 10 + (ch - L'0');
    }

    std::wstring result;
    result.reserve(mantissa.size() + 10);
    result.append(mantissa);
    result.append(L" \u00D7 10"); // × 10

    if (negative) result.push_back(L'\u207B'); // ⁻

    if (exponent == 0) {
        result.push_back(ToSuperscriptDigit(0));
    } else {
        wchar_t buf[16];
        int bufIdx = 15;
        buf[bufIdx--] = L'\0';
        int tmp = exponent;
        while (tmp > 0) {
            buf[bufIdx--] = ToSuperscriptDigit(tmp % 10);
            tmp /= 10;
        }
        result.append(buf + bufIdx + 1);
    }

    SSP_LOG_DEBUG("ScientificConverter: '%*ls' -> '%ls'",
        static_cast<int>(input.size()), input.data(), result.c_str());
    return result;
}

// ============================================================================
// SuperscriptBuilder
// ============================================================================

std::wstring SuperscriptBuilder::Convert(std::wstring_view input) {
    std::wstring result;
    result.reserve(input.size());
    bool changed = false;

    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t ch = input[i];
        if (ch == L'^' && i + 1 < input.size()) {
            size_t j = i + 1;

            // Check for parenthesized group: ^(...)
            if (input[j] == L'(') {
                size_t depth = 1;
                size_t k = j + 1;
                while (k < input.size() && depth > 0) {
                    if (input[k] == L'(') ++depth;
                    else if (input[k] == L')') --depth;
                    ++k;
                }
                if (depth == 0) {
                    result.push_back(ToSuperscriptChar(L'('));
                    for (size_t m = j + 1; m < k - 1; ++m) {
                        wchar_t inner = input[m];
                        if (HasSuperscriptForm(inner)) {
                            result.push_back(ToSuperscriptChar(inner));
                        } else {
                            result.push_back(inner);
                        }
                    }
                    result.push_back(ToSuperscriptChar(L')'));
                    i = k - 1;
                    changed = true;
                    continue;
                }
            }

            // Collect consecutive convertible chars
            size_t end = j;
            while (end < input.size() && HasSuperscriptForm(input[end])) {
                ++end;
            }

            if (end > j) {
                for (size_t m = j; m < end; ++m) {
                    result.push_back(ToSuperscriptChar(input[m]));
                }
                i = end - 1;
                changed = true;
                continue;
            }
        }
        result.push_back(ch);
    }

    SSP_LOG_DEBUG("SuperscriptBuilder: '%*ls' -> '%ls'%ls",
        static_cast<int>(input.size()), input.data(),
        result.c_str(), changed ? L"" : L" (unchanged)");
    return result;
}

// ============================================================================
// SubscriptBuilder
// ============================================================================

std::wstring SubscriptBuilder::Convert(std::wstring_view input) {
    std::wstring result;
    result.reserve(input.size());
    bool changed = false;

    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t ch = input[i];
        if (IsAsciiLetter(ch) && i + 1 < input.size() && IsAsciiDigit(input[i + 1])) {
            result.push_back(ch);
            ++i;
            while (i < input.size() && IsAsciiDigit(input[i])) {
                int d = input[i] - L'0';
                result.push_back(ToSubscriptDigit(d));
                ++i;
                changed = true;
            }
            --i; // back up so for-loop ++i lands on the first non-digit
            continue;
        }
        result.push_back(ch);
    }

    SSP_LOG_DEBUG("SubscriptBuilder: '%*ls' -> '%ls'%ls",
        static_cast<int>(input.size()), input.data(),
        result.c_str(), changed ? L"" : L" (unchanged)");
    return result;
}

// ============================================================================
// FractionBuilder
// ============================================================================

std::wstring FractionBuilder::Convert(std::wstring_view input) {
    static const std::unordered_map<std::wstring_view, wchar_t> kFractions = {
        { L"1/2",  L'\x00BD' }, // ½
        { L"1/3",  L'\x2153' }, // ⅓
        { L"2/3",  L'\x2154' }, // ⅔
        { L"1/4",  L'\x00BC' }, // ¼
        { L"3/4",  L'\x00BE' }, // ¾
        { L"1/5",  L'\x2155' }, // ⅕
        { L"2/5",  L'\x2156' }, // ⅖
        { L"3/5",  L'\x2157' }, // ⅗
        { L"4/5",  L'\x2158' }, // ⅘
        { L"1/6",  L'\x2159' }, // ⅙
        { L"5/6",  L'\x215A' }, // ⅚
        { L"1/8",  L'\x215B' }, // ⅛
        { L"3/8",  L'\x215C' }, // ⅜
        { L"5/8",  L'\x215D' }, // ⅝
        { L"7/8",  L'\x215E' }, // ⅞
    };

    auto it = kFractions.find(input);
    if (it != kFractions.end()) {
        std::wstring result(1, it->second);
        SSP_LOG_DEBUG("FractionBuilder: '%*ls' -> '%ls'",
            static_cast<int>(input.size()), input.data(), result.c_str());
        return result;
    }

    SSP_LOG_DEBUG("FractionBuilder: '%*ls' -> no match",
        static_cast<int>(input.size()), input.data());
    return std::wstring(input);
}

// ============================================================================
// LaTeXConverter
// ============================================================================

std::wstring LaTeXConverter::Convert(std::wstring_view input) {
    if (input.empty() || input[0] != L'\\') {
        return std::wstring(input);
    }

    // Normalize command to lowercase for case-insensitive lookup
    std::wstring lower;
    lower.reserve(input.size());
    for (auto ch : input) {
        lower.push_back(std::towlower(ch));
    }
    auto cmd = std::wstring_view(lower).substr(1); // strip leading '\'

    // Determine if original first letter after \ is uppercase
    bool isUpperGreek = (input.size() > 1 &&
        input[1] >= L'A' && input[1] <= L'Z');

    static const std::unordered_map<std::wstring_view, wchar_t> kGreekLower = {
        { L"alpha",      L'\x03B1' }, // α
        { L"beta",       L'\x03B2' }, // β
        { L"gamma",      L'\x03B3' }, // γ
        { L"delta",      L'\x03B4' }, // δ
        { L"epsilon",    L'\x03B5' }, // ε
        { L"zeta",       L'\x03B6' }, // ζ
        { L"eta",        L'\x03B7' }, // η
        { L"theta",      L'\x03B8' }, // θ
        { L"iota",       L'\x03B9' }, // ι
        { L"kappa",      L'\x03BA' }, // κ
        { L"lambda",     L'\x03BB' }, // λ
        { L"mu",         L'\x03BC' }, // μ
        { L"nu",         L'\x03BD' }, // ν
        { L"xi",         L'\x03BE' }, // ξ
        { L"pi",         L'\x03C0' }, // π
        { L"rho",        L'\x03C1' }, // ρ
        { L"sigma",      L'\x03C3' }, // σ
        { L"tau",        L'\x03C4' }, // τ
        { L"upsilon",    L'\x03C5' }, // υ
        { L"phi",        L'\x03C6' }, // φ
        { L"chi",        L'\x03C7' }, // χ
        { L"psi",        L'\x03C8' }, // ψ
        { L"omega",      L'\x03C9' }, // ω
    };

    static const std::unordered_map<std::wstring_view, wchar_t> kGreekUpper = {
        { L"gamma",      L'\x0393' }, // Γ
        { L"delta",      L'\x0394' }, // Δ
        { L"theta",      L'\x0398' }, // Θ
        { L"lambda",     L'\x039B' }, // Λ
        { L"xi",         L'\x039E' }, // Ξ
        { L"pi",         L'\x03A0' }, // Π
        { L"sigma",      L'\x03A3' }, // Σ
        { L"upsilon",    L'\x03A5' }, // Υ
        { L"phi",        L'\x03A6' }, // Φ
        { L"psi",        L'\x03A8' }, // Ψ
        { L"omega",      L'\x03A9' }, // Ω
    };

    static const std::unordered_map<std::wstring_view, wchar_t> kSymbols = {
        { L"sum",        L'\x2211' }, // ∑
        { L"int",        L'\x222B' }, // ∫
        { L"prod",       L'\x220F' }, // ∏
        { L"partial",    L'\x2202' }, // ∂
        { L"infty",      L'\x221E' }, // ∞
        { L"approx",     L'\x2248' }, // ≈
        { L"neq",        L'\x2260' }, // ≠
        { L"leq",        L'\x2264' }, // ≤
        { L"geq",        L'\x2265' }, // ≥
        { L"times",      L'\x00D7' }, // ×
        { L"div",        L'\x00F7' }, // ÷
        { L"pm",         L'\x00B1' }, // ±
        { L"sqrt",       L'\x221A' }, // √
        { L"nabla",      L'\x2207' }, // ∇
        { L"forall",     L'\x2200' }, // ∀
        { L"exists",     L'\x2203' }, // ∃
        { L"in",         L'\x2208' }, // ∈
        { L"notin",      L'\x2209' }, // ∉
        { L"subset",     L'\x2282' }, // ⊂
        { L"cup",        L'\x222A' }, // ∪
        { L"cap",        L'\x2229' }, // ∩
        { L"cdot",       L'\x22C5' }, // ⋅
        { L"rightarrow", L'\x2192' }, // →
        { L"leftarrow",  L'\x2190' }, // ←
    };

    // Try symbols first
    auto symIt = kSymbols.find(cmd);
    if (symIt != kSymbols.end()) {
        std::wstring result(1, symIt->second);
        SSP_LOG_DEBUG("LaTeXConverter: '%*ls' -> '%ls'",
            static_cast<int>(input.size()), input.data(), result.c_str());
        return result;
    }

    // Try uppercase Greek (only if original first letter is uppercase)
    if (isUpperGreek) {
        auto capIt = kGreekUpper.find(cmd);
        if (capIt != kGreekUpper.end()) {
            std::wstring result(1, capIt->second);
            SSP_LOG_DEBUG("LaTeXConverter: '%*ls' -> '%ls'",
                static_cast<int>(input.size()), input.data(), result.c_str());
            return result;
        }
    }

    // Try lowercase Greek
    auto lowIt = kGreekLower.find(cmd);
    if (lowIt != kGreekLower.end()) {
        std::wstring result(1, lowIt->second);
        SSP_LOG_DEBUG("LaTeXConverter: '%*ls' -> '%ls'",
            static_cast<int>(input.size()), input.data(), result.c_str());
        return result;
    }

    SSP_LOG_DEBUG("LaTeXConverter: '%*ls' -> no match",
        static_cast<int>(input.size()), input.data());
    return std::wstring(input);
}

} // namespace ssp
