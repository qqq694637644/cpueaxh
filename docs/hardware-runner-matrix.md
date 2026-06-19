# GitHub hosted runner feature matrix

The validation framework currently uses GitHub-hosted `windows-2022` as the required baseline. The test executable queries CPUID and OS-enabled state at runtime, then selects only generated specs whose feature gates are available on that runner.

This project does not require self-hosted hardware for the current regression gate. Optional CPU features vary across GitHub-hosted runners, so every run must preserve the observed feature matrix and generated spec manifest as evidence.

## Required hosted-runner evidence

The required `msvc-test` workflow records:

- `cpu-info.txt` from Windows runner inventory;
- `cpu-features.json` from `test.exe --dump-features`;
- `generated-specs.json` from `test.exe --dump-specs`;
- `test-specs.log` from `test.exe --list`;
- `manual-index.log` from `test.exe --list-manual`;
- `stage3-gates.log` from `test.exe --list-gates`;
- `manual-replay.log` from the manual coverage sample;
- `test-run.log` from the full regression suite;
- replay bundles when a recordable failure occurs.

## Test selection

The generated tests are feature-gated at runtime. Do not assume AES, SHA, AVX2, AVX-512, BMI, FMA, CET, RDRAND, RDSEED, XSAVE, or other optional features are present. The correct source of truth for a CI run is the pair of files:

```powershell
.\x64\Release\test.exe --dump-features cpu-features.json
.\x64\Release\test.exe --dump-specs generated-specs.json
```

For longer hosted fuzz, use the scheduled/manual `extended-regression.yml` workflow or run:

```powershell
.\x64\Release\test.exe --generated-seeds 512 --record-bundle failure-bundle
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
4. explicit `unsafe_for_native` status in `docs/instruction-status.yml`.

The current PR gate is intentionally hosted-runner-only and should not introduce self-hosted hardware requirements.
