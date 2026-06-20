# Instruction test generator template

Use this template when adding a new generated true-CPU differential instruction case.

## 1. Status entry

Add or update `docs/instruction-status.json` at form granularity:

```yaml
- mnemonic: ADD
  forms:
    - name: add_rr_rax_rbx
      encoding: "REX.W 01 /r"
      operands: ["r/m64", "r64"]
      operand_size: 64
      mode: long64
      feature_gate: base
      status: implemented
      coverage:
        generated_differential: true
        manual: false
        regression_replay: false
      unsafe_native_reason: null
```

Do not mark a whole mnemonic as complete unless every intended form is listed and covered.

## 2. ProgramSpec

Add one small `ProgramSpec` entry in `make_specs()` or extend an existing family only when the encoding shape is already represented safely.

Required naming rules:

- Names must be stable and exact-selector friendly.
- Names should include operation, operand kind, and important register/memory variant.
- Do not rely on substring filters for replay.

Example:

```cpp
specs.push_back({ Family::BinaryRegReg, static_cast<std::uint32_t>(BinaryOp::Add), 0, kStatusMask, "add_rr_rax_rbx" });
```

## 3. Case builder

The generated code should:

1. Set up deterministic initial data from `seed`.
2. Emit exactly the instruction sequence being tested plus a safe return path.
3. Store SIMD/x87 results to the data area when direct register comparison is not supported.
4. Avoid privileged, blocking, nondeterministic, or unsafe-native instructions.

## 4. Flags

Set `flag_mask` to architectural defined outputs only. Undefined or unchanged flags must not be compared.

## 5. Failure reproduction

A generated mismatch should write `failure.json` with:

```json
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "<exact generated spec name>",
  "seed_index": 0,
  "replay_hint": "test.exe --case <exact generated spec name> --seed-index 0"
}
```

## 6. Regression corpus

After fixing a bug, add the minimized generated replay record to `test/regression/*.json` so default full regression replays it permanently.
