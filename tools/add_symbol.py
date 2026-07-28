#!/usr/bin/env python3
"""
Add a symbol to the Scientific Symbol Panel database.

Usage:
    python tools/add_symbol.py <SYMBOL> <NAME> <CATEGORY> [options]

Examples:
    python tools/add_symbol.py "∞" "Infinity" "Mathematics"
    python tools/add_symbol.py "⚡" "Lightning" "Miscellaneous" --latex "\\lightning" --aliases "bolt,thunder"
    python tools/add_symbol.py "ℏ" "Planck Constant" "Physics" -a "hbar,planck" -k "quantum,reduced"

Categories: Mathematics, Greek Letters, Physics, Chemistry, Electronics,
            SI Units, Logic, Programming, Arrows, Currency, Fractions,
            Superscripts, Subscripts, Statistics, Geometry, Calculus,
            Astronomy, Miscellaneous, Custom

The script appends the symbol to data/symbols.json and regenerates data/symbols.bin.
"""

import argparse
import json
import os
import struct
import sys
from pathlib import Path

# Must match src/Core/Types.h Category enum order
CATEGORIES = [
    "Mathematics", "Greek Letters", "Physics", "Chemistry",
    "Electronics", "SI Units", "Logic", "Programming",
    "Arrows", "Currency", "Fractions", "Superscripts",
    "Subscripts", "Statistics", "Geometry", "Calculus",
    "Astronomy", "Miscellaneous", "Custom",
]

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DATA_DIR = PROJECT_ROOT / "data"
SYMBOLS_JSON = DATA_DIR / "symbols.json"
SYMBOLS_BIN = DATA_DIR / "symbols.bin"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Add a symbol to Scientific Symbol Panel",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Categories:\n  " + "\n  ".join(CATEGORIES),
    )
    parser.add_argument("symbol", nargs="?", help="The Unicode symbol character (e.g. '∞')")
    parser.add_argument("name", nargs="?", help="Display name (e.g. 'Infinity')")
    parser.add_argument("category", nargs="?", help="Category name (see list below)")
    parser.add_argument(
        "-a", "--aliases",
        help="Comma-separated search aliases (e.g. 'pi,3.14')",
    )
    parser.add_argument(
        "-k", "--keywords",
        help="Comma-separated keywords for search ranking",
    )
    parser.add_argument(
        "-l", "--latex",
        help="LaTeX command (e.g. '\\\\infty')",
    )
    parser.add_argument(
        "--html",
        help="HTML entity (e.g. '&infin;')",
    )
    parser.add_argument(
        "-d", "--description",
        help="Human-readable description",
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Regenerate symbols.bin from symbols.json (no new symbol added)",
    )
    return parser.parse_args()


def validate(args):
    if len(args.symbol) == 0:
        sys.exit("Error: symbol must be at least one character")

    codepoint = ord(args.symbol[0])
    if len(args.symbol) > 1 and not all(0x300 <= ord(c) <= 0x36F or 0xFE00 <= ord(c) <= 0xFE0F for c in args.symbol[1:]):
        print(f"Warning: multi-character symbol '{args.symbol}' — using only first character (U+{codepoint:04X})")

    if args.category not in CATEGORIES:
        print(f"Error: unknown category '{args.category}'")
        print(f"Valid categories: {', '.join(CATEGORIES)}")
        sys.exit(1)

    return codepoint


def load_symbols():
    with open(SYMBOLS_JSON, "r", encoding="utf-8") as f:
        return json.load(f)


def save_symbols(symbols):
    with open(SYMBOLS_JSON, "w", encoding="utf-8") as f:
        json.dump(symbols, f, indent=2, ensure_ascii=False)
        f.write("\n")


def check_duplicate(symbols, codepoint, name):
    for s in symbols:
        if s["codepoint"] == codepoint:
            print(f"Warning: codepoint U+{codepoint:04X} already exists as '{s['name']}'")
            return True
        if s["name"].lower() == name.lower():
            print(f"Warning: name '{name}' already exists for symbol '{s['symbol']}'")
    return False


def build_symbol_entry(codepoint, args):
    aliases = [a.strip() for a in args.aliases.split(",")] if args.aliases else []
    keywords = [k.strip() for k in args.keywords.split(",")] if args.keywords else []

    entry = {
        "symbol": args.symbol,
        "codepoint": codepoint,
        "name": args.name,
        "aliases": aliases,
        "keywords": keywords,
        "category": args.category,
        "latex": args.latex or "",
        "htmlEntity": args.html or "",
        "description": args.description or "",
    }
    return entry


def write_binary(symbols):
    """Regenerate symbols.bin from the full symbol list."""
    cat_map = {name: i for i, name in enumerate(CATEGORIES)}

    buf = bytearray()
    # Magic + version + count
    buf.extend(struct.pack("<III", 0x44505353, 1, len(symbols)))

    for s in symbols:
        category = cat_map.get(s["category"], 18)  # default to Custom

        buf.extend(struct.pack("<I", s["codepoint"]))
        buf.append(category)

        # Helper: write a UTF-16LE length-prefixed string
        def write_wstr(data, text):
            encoded = text.encode("utf-16-le") if text else b""
            char_count = len(encoded) // 2
            data.extend(struct.pack("<H", char_count))
            data.extend(encoded)

        # Helper: write a vector of wstrs
        def write_wstr_vec(data, texts):
            data.extend(struct.pack("<H", len(texts)))
            for t in texts:
                write_wstr(data, t)

        write_wstr(buf, s["symbol"])
        write_wstr(buf, s["name"])
        write_wstr_vec(buf, s.get("aliases", []))
        write_wstr_vec(buf, s.get("keywords", []))
        write_wstr(buf, s.get("latex", ""))
        write_wstr(buf, s.get("htmlEntity", ""))
        write_wstr(buf, s.get("description", ""))

    with open(SYMBOLS_BIN, "wb") as f:
        f.write(buf)


def main():
    args = parse_args()

    if not SYMBOLS_JSON.exists():
        sys.exit(f"Error: {SYMBOLS_JSON} not found — run from project root")

    if args.rebuild:
        symbols = load_symbols()
        write_binary(symbols)
        print(f"Regenerated {SYMBOLS_BIN} from {SYMBOLS_JSON} ({len(symbols)} symbols)")
        return

    if not args.symbol or not args.name or not args.category:
        sys.exit("Error: SYMBOL, NAME, and CATEGORY are required (or use --rebuild)")

    codepoint = validate(args)
    symbols = load_symbols()

    # Check duplicates (warn, don't block)
    check_duplicate(symbols, codepoint, args.name)

    # Build and append
    entry = build_symbol_entry(codepoint, args)
    symbols.append(entry)

    # Save JSON
    save_symbols(symbols)
    print(f"Added '{args.symbol}' ({args.name}) to {SYMBOLS_JSON}")

    # Regenerate binary
    write_binary(symbols)
    print(f"Regenerated {SYMBOLS_BIN} ({len(symbols)} symbols)")

    print(f"\nDone. Rebuild the project to pick up changes.")


if __name__ == "__main__":
    main()
