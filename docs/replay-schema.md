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
- `initial_state`: diagnostic only; replay regenerates the case state from `case_selector` and `seed_index`.
- `result_state`: diagnostic only; replay recomputes native/emu results from `case_selector` and `seed_index`.
- `host_features`: diagnostic only; the run-time feature matrix is also written separately to `cpu-features.json` when `--record-bundle` is used.
- `detail`: diagnostic only.
- `case_name`: diagnostic only.
- `replay_hint`: diagnostic only.

Generated failures emitted by `--record-failure` / `--record-bundle` include an `initial_state` object when the runner can attach generated case metadata:

```json
{
  "schema": "cpueaxh.generated-initial-state.v1",
  "gprs": {
    "rax": "0x0000000000000000"
  },
  "rip": "0x0000000000000000",
  "rflags": "0x0000000000000246",
  "mxcsr": "0x0000000000001f80",
  "xmm": [
    "0x00000000000000000000000000000000"
  ],
  "data_offset": 64,
  "data_hex": "01 02 03"
}
```

This snapshot is for debugging and minimization. It must not replace deterministic replay from `case_selector + seed_index`.

Generated differential mismatches also include a `result_state` object after both native and emulated executions have produced readable state:

```json
{
  "schema": "cpueaxh.generated-result-state.v1",
  "native_result": {
    "gprs": {
      "rax": "0x0000000000000000"
    },
    "rip": "0x0000000000100000",
    "rflags": "0x0000000000000246",
    "mxcsr": "0x0000000000001f80",
    "data_hex": "01 02 03"
  },
  "emu_result": {
    "gprs": {
      "rax": "0x0000000000000000"
    },
    "rip": "0x0000000000100000",
    "rflags": "0x0000000000000246",
    "mxcsr": "0x0000000000001f80",
    "data_hex": "01 02 03"
  }
}
```

Native code and stack pointers are normalized to guest addresses before they are written. This result snapshot is diagnostic evidence for the observed mismatch and is not a replay source of truth.

Failure records also include `host_features` when the runner can attach the same feature matrix used for generated test selection:

```json
{
  "schema": "cpueaxh.host-features.v1",
  "vendor": "GenuineIntel",
  "brand": "Intel(R) Xeon(R) Platinum ...",
  "max_leaf": 32,
  "max_leaf7": 2,
  "max_extended_leaf": 2147483656,
  "family": 6,
  "model": 85,
  "stepping": 7,
  "features": {
    "avx": true,
    "avx2": true,
    "aes": true
  }
}
```

This is a copy of the feature-gate evidence needed to interpret `generated-specs.json` and the selected generated tests. The family/model/stepping fields are derived from CPUID leaf 1, and the brand string is derived from extended CPUID leaves when available. It is not a claim that unlisted optional CPU features were tested.

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
  "vendor": "GenuineIntel",
  "brand": "Intel(R) Xeon(R) Platinum ...",
  "max_leaf": 32,
  "max_leaf7": 2,
  "max_extended_leaf": 2147483656,
  "family": 6,
  "model": 85,
  "stepping": 7,
  "features": {
    "avx": true,
    "avx2": true,
    "aes": true
  }
}
```

The record is intentionally derived from `query_host_features()` rather than only from OS inventory APIs, so it reflects both CPUID and OS-enabled state where required. The `brand` field comes from extended CPUID leaves when available; feature booleans remain the authoritative source for generated-test selection.

## Generated spec manifest

`test.exe --dump-specs <path>` writes the generated differential spec manifest selected under the current host feature matrix:

```json
{
  "schema": "cpueaxh.generated-specs.v1",
  "spec_count": 123,
  "features": {
    "avx": true
  },
  "specs": [
    {
      "name": "add_rr_rax_rbx",
      "family": "binary_reg_reg",
      "op": 0,
      "variant": 0,
      "flag_mask": 2261
    }
  ]
}
```

CI validates this manifest with `tools/validate-generated-spec-manifest.ps1`. The validator checks schema, count, unique spec names, required baseline specs, and that every generated replay corpus `case_selector` exists in the manifest.

## Failure bundle

`test.exe --record-bundle <dir>` writes structured diagnostics into a directory. The current bundle contains:

- `cpu-features.json`: written at test start with schema `cpueaxh.host-features.v1`.
- `generated-specs.json`: written at test start with schema `cpueaxh.generated-specs.v1`.
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

Manual replay records are rejected at runtime unless all of these conditions hold:

- `category` exists and is either `manual` or `unsafe-native`;
- `category` matches the category in `test.exe --list-manual` for the selected `case_selector`;
- `replay` exists and contains `--manual-case <case_selector>`.

This is intentionally strict: malformed manual replay records must be fixed rather than silently falling back to a broader mode.

Equivalent command:

```powershell
.\x64\Release\test.exe --manual-case exception_priority --record-bundle failure-bundle
```

## Failure artifact bundle

On CI failure, the workflow uploads a diagnostics bundle containing:

- `failure-bundle/**`, including `failure.json` when emitted and `cpu-features.json`.
- `cpu-features.json` dumped before the full run.
- `generated-specs.json` dumped before the full run.
- `test-specs.log` from `test.exe --list`.
- `manual-index.log` from `test.exe --list-manual`.
- `test-run.log` from the full regression run.
- `cpu-info.txt` from the Windows runner.

These files are enough to identify the failing generated case and reproduce it locally when the failure is generated-case based. Generated failures also include initial and result scalar/vector/data snapshots for debugging and minimization.
