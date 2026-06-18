# Regression corpus

This directory stores minimized generated-case replay records that must keep passing after bugs are fixed.

The default full test run automatically loads `test/regression/*.json` and replays each generated case after the generated differential suite and manual special cases:

```powershell
.\x64\Release\test.exe --record-failure failure.json
```

To replay one recorded generated case directly:

```powershell
.\x64\Release\test.exe --replay test\regression\add_rr_rax_rbx_seed0.json
```

Replay JSON files currently require an exact generated spec selector and a seed index:

```json
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "replay_hint": "test.exe --case add_rr_rax_rbx --seed-index 0"
}
```

Manual or unsafe-native cases still need explicit C++ test coverage until a richer replay schema is added.

See also:

- `docs/replay-schema.md` for the generated replay contract.
- `docs/instruction-test-generator-template.md` for adding new generated differential cases.
- `docs/hardware-runner-matrix.md` for long-running and self-hosted hardware validation guidance.
