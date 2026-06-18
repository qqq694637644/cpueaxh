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

## Manual / unsafe-native index records

Manual and unsafe-native cases should be indexed by name and coverage category. They are not directly replayed by `--replay` yet.

Use `test.exe --list-manual` to list the current manual/unsafe-native coverage index.

Recommended future record shape:

```json
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "memory, stack, canonical-address, and UD exception ordering",
  "replay": "explicit-cpp"
}
```

## Failure artifact bundle

On CI failure, the workflow uploads a diagnostics bundle containing:

- `failure.json` when the runner emitted one.
- `test-specs.log` from `test.exe --list`.
- `manual-index.log` from `test.exe --list-manual`.
- `test-run.log` from the full regression run.
- `cpu-info.txt` from the Windows runner.

These files are enough to identify the failing generated case and reproduce it locally when the failure is generated-case based. Future work may add machine-state snapshots or minimized replay bundles.
