## Summary

- 

## Instruction / test status

- [ ] No instruction behavior changed.
- [ ] Instruction behavior changed and `docs/instruction-status.yml` was updated.
- [ ] New or changed user-mode safe behavior has true-CPU differential coverage.
- [ ] Unsafe, privileged, model-only, or controlled-runner behavior is explicitly documented in `docs/instruction-status.yml`.

## Regression validation

- [ ] Targeted test run:

  ```powershell
  .\x64\Release\test.exe --case <exact-name> --seed-index <n> --record-bundle failure-bundle
  ```

- [ ] Full regression run:

  ```powershell
  .\x64\Release\test.exe --record-bundle failure-bundle
  ```

- [ ] Relevant `test/regression/*.json` replay records were added or updated for fixed bugs.
- [ ] CI passed on the latest commit.

## Risk notes

List high-risk files touched, especially decoder, executor, dispatch, memory, flags, or shared helper code.

- 
