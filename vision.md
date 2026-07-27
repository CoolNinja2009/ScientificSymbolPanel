# Scientific Symbol Panel

> A native Windows experience for STEM symbols, powered by semantic search and local AI.

## Vision

The Scientific Symbol Panel is designed to become the native Windows experience for technical and scientific workflows. It combines fast symbol lookup, offline-first behavior, and optional local intelligence to support students, engineers, researchers, and power users.

## Core Principles

- Fast popup experience
- Lightweight footprint
- Offline-first design
- Open source philosophy
- Plugin-driven architecture
- Optional local AI with privacy in mind

## Roadmap

### Phase 1 — Foundation (v1.0)

#### Goal
Build a reliable native symbol picker for everyday use.

#### Core capabilities
- Native Win32 + Direct2D interface
- Alt+A popup trigger
- Instant symbol search
- Category browsing
- Favorites and recent symbols
- Copy to clipboard
- Direct insertion via SendInput
- Unicode support
- High DPI compatibility
- Dark and light themes
- Persistent settings
- Keyboard and mouse navigation
- Search aliases and ranking
- Plugin loader
- JSON-based symbol database

#### Performance targets
- Startup: under 30 ms
- Popup: under 10 ms
- Search: under 1 ms

### Phase 2 — Smart Search (v1.5)

No AI yet. The focus is on better metadata quality and more intuitive matching.

#### Planned improvements
- Richer aliases for technical terms
- Support for current-related symbols such as A, I, mA, and µA
- Plural handling for terms such as integrals
- Abbreviation support for terms such as temp
- Misspelling tolerance such as resitance → Ω

Example concept:

```json
{
  "aliases": [
    "ohm",
    "resistance",
    "resistor",
    "electrical resistance",
    "impedance"
  ]
}
```

### Phase 3 — Technical Writing (v2.0)

Expand beyond symbol lookup into reusable technical writing tools.

#### Planned features
- Snippets for formulas and technical text
- Scientific notation support
- Equation snippets such as quadratic formula, Euler identity, ideal gas law, Kirchhoff's laws, and Maxwell equations
- LaTeX mode for converting expressions like \\alpha into α
- Unit builder for entries like microfarad → µF

### Phase 4 — Semantic Search (v3.0)

This phase introduces semantic understanding without relying on a full LLM.

#### Planned capabilities
- Embedding-based search
- Offline-compatible model size of roughly 10–30 MB
- Optional local inference
- Search for concepts such as:
  - “thing used to measure current” → A, mA, µA, ammeter
  - “quantum mechanics” → ℏ, ψ, ∂, ∇
  - “set theory” → ∈, ∉, ⊂, ⊆, ∪, ∩
  - “boolean algebra” → ∧, ∨, ⊻, ¬
  - “electricity” → related clusters such as Ω, V, A, W, Hz, µ

### Phase 5 — AI Assistant (v3.5)

A small local AI layer helps interpret user intent rather than simply matching text.

#### Example interactions
- “How do I write resistance?” → Ω
- “Symbol for average” → μ
- “Vector calculus” → ∇, ∂, ∫

### Phase 6 — Formula Builder (v4)

A visual formula builder makes it easier to construct expressions without manually typing them.

#### Example building blocks
- Fraction
- Square root
- Summation
- Subscript

The result is exported as Unicode text.

### Phase 7 — OCR

Allow users to capture a screenshot of an equation or symbol and convert it into Unicode or LaTeX.

### Phase 8 — AI Knowledge Graph

Instead of storing only a symbol, the system can store relationships between concepts and symbols.

#### Example relationships
- Resistance → Ohm → Electrical Engineering → Kirchhoff → Circuit Analysis → Voltage → Current

This enables concept-driven search, such as searching “Kirchhoff” and retrieving related symbols like Ω, V, I, and Σ.

### Phase 9 — Plugin Marketplace

Support community-created packs for specialized domains.

#### Example pack categories
- Electrical engineering
- Physics
- Mathematics
- Astronomy
- Chemistry
- Unicode
- Programming
- Logic
- Medical symbols
- Music theory
- Statistics

Each pack may include:
- symbols.json
- aliases.json
- snippets.json
- metadata.json

### Phase 10 — AI-Generated Packs

Users can request a new pack, and AI can generate the required symbols, aliases, snippets, and categories automatically.

Example request:

> Create a pack for tensor calculus.

### Phase 11 — Clipboard Intelligence

Future versions may analyze clipboard content and offer intelligent symbol or snippet suggestions based on what the user is working with.