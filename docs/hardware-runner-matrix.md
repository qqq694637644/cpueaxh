# Hardware runner matrix

The default PR gate uses GitHub-hosted `windows-2022` and should remain the required baseline. It cannot prove full AMD64 feature coverage because runner CPU features vary and some instructions are unsafe to execute in ordinary user mode.

## Recommended self-hosted labels

Use dedicated Windows x64 machines with stable labels:

| Label | Purpose |
| --- | --- |
| `self-hosted-windows-intel-sse2-baseline` | baseline integer/SSE2 coverage |
| `self-hosted-windows-intel-avx2` | AVX/AVX2/FMA/BMI coverage |
| `self-hosted-windows-amd-zen3` | AMD long-mode and feature behavior comparison |
| `self-hosted-windows-amd-zen4` | newer AMD feature behavior comparison |
| `self-hosted-windows-intel-avx512` | AVX-512 and extended vector behavior |
| `self-hosted-windows-controlled-system` | privileged/model/controlled-environment experiments only |

## Manual workflow entry

`.github/workflows/hardware-matrix-regression.yml` provides a manual `workflow_dispatch` entry point for self-hosted hardware runs. It is intentionally not scheduled and is not part of the default hosted PR gate.

Dispatch inputs:

| Input | Purpose |
| --- | --- |
| `runner_labels_json` | JSON `runs-on` label array, for example `["self-hosted","windows","x64","self-hosted-windows-intel-avx2"]` |
| `generated_seeds` | generated seed count for long fuzz, default `512` |
| `case_filter` | optional generated spec substring filter |
| `include_manual_replay` | whether to replay `test/manual/exception_priority.json` |

The workflow runs the same regression contract validators as hosted CI, then records CPU feature evidence, generated spec manifest, stage 3 gates, optional manual replay, and the long generated regression bundle.

## Test selection

Every runner should query CPUID at runtime and only execute tests whose features are supported. Do not assume AES, SHA, AVX2, AVX-512, BMI, FMA, CET, RDRAND, RDSEED, XSAVE, or other optional features are present.

Recommended commands:

```powershell
.\x64\Release\test.exe --dump-features cpu-features.json
.\x64\Release\test.exe --dump-specs generated-specs.json
.\x64\Release\test.exe --record-bundle failure-bundle
.\x64\Release\test.exe --generated-seeds 512 --record-bundle failure-bundle
.\x64\Release\test.exe --list-manual
```

## Unsafe-native policy

These instruction classes must not be executed directly in normal GitHub-hosted user-mode CI:

- privileged instructions;
- I/O port instructions;
- MSR access;
- VMX/SVM virtualization instructions;
- system instructions whose architectural behavior depends on kernel, paging, descriptor-table, or CPL state.

They require one of these strategies:

1. software/model tests;
2. escape callback tests;
3. manual special tests;
4. controlled self-hosted machine tests;
5. explicit `unsafe_for_native` status in `docs/instruction-status.yml`.

## Required evidence

Self-hosted runs should preserve:

- runner label and machine class;
- CPU vendor/model/features;
- test command line;
- failure JSON if any;
- `cpu-features.json` from `test.exe --dump-features` or `--record-bundle`;
- `generated-specs.json` from `test.exe --dump-specs` or `--record-bundle`;
- full test log;
- minimized replay or manual case identifier.
