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

## Test selection

Every runner should query CPUID at runtime and only execute tests whose features are supported. Do not assume AES, SHA, AVX2, AVX-512, BMI, FMA, CET, RDRAND, RDSEED, XSAVE, or other optional features are present.

Recommended commands:

```powershell
.\x64\Release\test.exe --record-failure failure.json
.\x64\Release\test.exe --generated-seeds 512 --record-failure failure.json
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
- full test log;
- minimized replay or manual case identifier.
