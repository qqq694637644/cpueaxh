# Manual / unsafe-native replay records

This directory stores structured replay records for manual or unsafe-native coverage groups.

Unlike generated replay records in `test/regression/*.json`, these records are not exact single-instruction generated cases. They point at named coverage groups from `test.exe --list-manual`.

Current replay behavior is conservative: `test.exe --replay <manual-record.json>` validates that the `case_selector` exists in the manual index and then runs the full manual special suite. This avoids false confidence while still giving CI and reviewers a structured replay entry point for manual coverage.

Example:

```powershell
.\x64\Release\test.exe --replay test\manual\exception_priority.json --record-bundle failure-bundle
```

A manual replay record requires:

```json
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "memory, stack, canonical-address, and UD exception ordering",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle"
}
```

The executable rejects malformed manual records at replay time. `category` must be `manual` or `unsafe-native`, it must match the category printed by `test.exe --list-manual` for the selected `case_selector`, and `replay` must contain `--manual-case <case_selector>`.
