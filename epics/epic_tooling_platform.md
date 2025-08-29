# Epic: Tooling & Developer Platform

## Goal
Deliver a cohesive developer experience: command line tools, editors, debuggers, and inspection utilities that make building and iterating polyglot modules fast and intuitive (JIT and AOT).

## Why It Matters
- Great tooling accelerates language + macro authoring and onboarding.
- A consistent CLI / API surface lets users script builds, tests, and experiments.
- LSP + DAP integration unlocks mainstream editor adoption and deep diagnostics.

## Success Criteria
- Unified CLI (e.g. `edn`) with subcommands: `parse`, `expand`, `lint`, `build`, `jit-run`, `aot`, `inspect-ir`, `diag-json`.
- LSP server providing: diagnostics on save, hover type info, go-to symbol, macro expansion preview, code actions (quick fixes), semantic tokens.
- DAP integration supporting stepping through mixed-language frames with unified stack + source maps into macro expansions.
- JIT session manager: hot-reload modules, evaluate expressions, list loaded symbols.
- AOT pipeline producing static/shared libs plus optional metadata bundle (for debugging & tooling).

## Key Tracks & Steps
### 1. Core CLI
1. Spec CLI command structure & flags (doc first).
2. Implement `parse` (raw AST pretty print + error codes).
3. Implement `expand` (macro expansion stages, diff mode, timing stats).
4. Implement `build` / `jit-run` (compile & execute entry fn, pass args).
5. Implement `inspect-ir` (filter by function, demangle, show traits / sums).
6. Add `--json` output mode for machine consumption.
7. Snapshot tests for each command.

### 2. JIT Orchestration
1. Persistent JIT session tool (`edn repl`): load/unload modules, list symbols.
2. Hot reload detection: checksum IR; only relink changed functions.
3. Expression evaluator: parse mini-surface snippet -> expand -> inject -> run.
4. Performance counters: track compile & execute timings.

### 3. AOT Pipeline
1. Emit object + DWARF/CodeView debug info toggle.
2. Bundle macro-expansion map (source surface -> core IR spans) for tooling.
3. Produce static (`.a`) and shared (`.dylib/.so`) outputs.
4. Verify reproducible builds (hash comparison) under canonical flags.

### 4. LSP Server
1. Protocol scaffold (initialize, open/close, diagnostics push).
2. Incremental parse cache keyed by doc version.
3. Hover: show inferred/core types + originating macro.
4. Go-to definition: surface symbol -> macro origin -> IR anchor.
5. Code actions: quick fix suggestions from hints (e.g., type mismatch alternatives).
6. Macro expansion preview virtual document (`edn:expanded/<file>` scheme).
7. Semantic tokens: classify constructs (macro, intrinsic, trait, sum variant).
8. Performance budget: <50ms typical incremental response.

### 5. DAP (Debugger) Integration
1. IR ↔ source (surface + macro) location map generation.
2. Breakpoints in surface code hit in mixed-language stacks.
3. Step over/into macro-expanded frames logically (collapse option).
4. Variable scopes & pretty printers (sums, traits, closures).
5. Evaluate expression in paused frame (surface snippet -> JIT inject).

### 6. Visualization & Introspection
1. Graph export: macro expansion DAG / IR CFG (`dot` / JSON).
2. Trait & sum layout inspectors.
3. Memory layout explorer (struct/enum/closure) with offsets.
4. Timeline profiler: per-pass duration + code size deltas.

### 7. Quality & Distribution
1. Telemetry opt-in (anon stats: expansion time, pass counts) with privacy doc.
2. Crash reporter (symbolized stack with IR dump snippet).
3. Pre-built binaries (GitHub Actions) for macOS/Linux.
4. Versioned JSON schema for diagnostics & IR metadata.

## Risks & Mitigations
- Complexity creep: enforce minimal viable subset per track before expanding.
- Latency in LSP: incremental AST + memoized macro expansions.
- Debug mapping accuracy: early invest in stable span propagation.

## Out of Scope (Initial)
- Full-blown package manager.
- Multi-node remote build farm integration.
- Web-based IDE.

## Metrics
- Time to first successful macro expansion (<5s fresh clone).
- Median LSP hover latency (<30ms).
- JIT hot reload turnaround (<150ms for small module change).
- Crash rate (goal: <0.1% tool invocations).

## Deliverables
- `docs/TOOLING.md` (living spec)
- CLI man page / `--help` coverage >=95%
- LSP & DAP protocol capability matrix
- Sample screencast scripts (README links)

## Follow-Up Ideas
- WASM-hosted LSP for browser playground.
- Deterministic build cache daemon.
- Unified perf explorer UI (latency + memory + size diff overlays).
