# Cpueaxh failure and replay schema

This document defines the replay records consumed by `test.exe --replay <path>` and the failure records emitted by `test.exe --record-failure <path>`.

## Scope

The current executable replay path is intentionally limited to generated true-CPU differential cases. Manual, model-only, privileged, unsafe-native, or controlled-runner cases are indexed but still require explicit C++ coverage or a future richer runner.

## Generated replay record

Required fields:

```json
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0
}
```

`case_selector` must be a non-empty exact generated spec name. `seed_index` must be an unquoted JSON number. Quoted numbers, negative values, signed values, and overflow values are rejected.

Accepted diagnostic fields:

- `seed`: diagnostic only; replay derives the deterministic seed from `case_selector` and `seed_index`.
- `image_hex`: diagnostic only; replay regenerates the case image from the current generator.
- `detail`: diagnostic only.
- `case_name`: diagnostic only.
- `replay_hint`: diagnostic only.

Replay command:

```powershell
.\x64\Release\test.exe --replay test\regression\add_rr_rax_rbx_seed0.json
```

Equivalent exact selector command:

```powershell
.\x64\Release\test.exe --case add_rr_rax_rbx --seed-index 0
```

`--replay` is mutually exclusive with list, selector, seed, generated-seed-count, and skip options. This prevents a replay record from silently overriding command-line selectors.

List modes are also strict: `--list-manual` must be used alone, while `--list` may only be combined with generated spec selectors (`--case`, `--filter-exact`, or `--filter`).

Long generated fuzz failures are also directly reproducible by the emitted `replay_hint`: `--seed-index <n>` runs a single deterministic seed and is not limited by the default full-suite seed count.

The default full suite fails if `test/regression/` is missing, not a directory, cannot be enumerated, or contains no replay JSON files. This prevents the corpus from silently disappearing.

## Host feature record

`test.exe --dump-features <path>` writes the feature matrix that the test executable itself will use for feature-gated generated cases:

```json
{
  "schema": "cpueaxh.host-features.v1",
  "features": {
    "avx": true,
    "avx2": true,
    "aes": true
  }
}
```

The record is intentionally derived from `query_host_features()` rather than only from OS inventory APIs, so it reflects both CPUID and OS-enabled state where required.

## Failure bundle

`test.exe --record-bundle <dir>` writes structured diagnostics into a directory. The current bundle contains:

- `cpu-features.json`: written at test start with schema `cpueaxh.host-features.v1`.
- `failure.json`: written only when the runner can identify a failing case.

This option is useful for CI artifacts because it keeps failure data and feature-gate evidence together. `--record-bundle` can be used with normal full runs, generated long fuzz, and `--replay`.

## Manual / unsafe-native index records

Manual and unsafe-native cases are indexed by name and coverage category. They can be replayed through `test.exe --replay <path>` when the record uses the `cpueaxh.manual-index.v1` schema.

Use `test.exe --list-manual` to list the current manual/unsafe-native coverage index.

Current record shape:

```json
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "memory, stack, canonical-address, and UD exception ordering",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle"
}
```

Current manual replay is intentionally conservative: the runner validates that `case_selector` exists in the manual index and then executes the full manual special suite. This avoids false confidence while still giving CI and reviewers a structured replay entry point for manual or unsafe-native coverage groups.

Equivalent command:

```powershell
.\x64\Release\test.exe --manual-case exception_priority --record-bundle failure-bundle
```

## Failure artifact bundle

On CI failure, the workflow uploads a diagnostics bundle containing:

- `failure-bundle/**`, including `failure.json` when emitted and `cpu-features.json`.
- `cpu-features.json` dumped before the full run.
- `test-specs.log` from `test.exe --list`.
- `manual-index.log` from `test.exe --list-manual`.
- `test-run.log` from the full regression run.
- `cpu-info.txt` from the Windows runner.

These files are enough to identify the failing generated case and reproduce it locally when the failure is generated-case based. Future work may add machine-state snapshots or minimized replay bundles.
