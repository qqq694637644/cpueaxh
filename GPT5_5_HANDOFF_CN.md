# GPT-5.5 交接文档：Cpueaxh 后续开发与测试框架守则

本文档写给后续接手本仓库的 GPT-5.5 或同等级代码维护模型。目标不是重新解释所有源码，而是固定开发方向、测试路径和风险边界，避免后续开发时把“实现指令”偏移成“大范围重构、弱化测试或绕过回归”。

## 0. 第一原则

`Cpueaxh` 是一个正在扩展的 x86-64 CPU emulator。后续任何开发都必须优先保护已有指令可靠性，而不是追求一次性补全更多指令。

每次开发都按这个顺序推进：

```text
先确认一个小任务
→ 阅读相邻实现和测试契约
→ 先补或定位测试覆盖
→ 做最小实现
→ 跑定向验证
→ 跑完整回归或说明无法运行
→ 提交 PR
→ 等 CI 证据
```

禁止把任务扩大成“顺手整理架构”“重写 decoder”“统一重构指令族”“降低 flag mask”“删除失败测试”。

## 1. GPT-5.5 接手时的行为约束

GPT-5.5 具备较强长上下文推理能力，但在维护此仓库时仍必须用工具和文件证据约束自己。不要凭记忆补全仓库状态，不要把上一次会话中的推断当成当前事实。

### 1.1 必须做的事

- 修改仓库前，先创建或使用 `gpt/*` 工作分支；不要直接改 `main`。
- 每次任务至少读取一次当前仓库结构、相关源码、相关测试或文档。
- 只改与任务直接相关的文件。
- 提交前查看 diff。
- 能跑测试就跑最相关测试；无法跑要明确说明原因。
- 创建或更新 PR 后查询 CI；找不到 run 就说“未找到匹配 run”，不要声称通过。
- 输出结论时给真实证据：分支、commit、PR、测试命令、CI 状态。

### 1.2 GPT-5.5 容易出现的偏移，需要主动抑制

- **过度概括**：看到 `README.md` 说长期目标是完整 x86-64，不代表当前任务可以一次补大批指令。
- **测试替代理解**：不要用“代码看起来正确”替代 `test.exe`、replay、CI artifact。
- **架构重写冲动**：decoder/executor/shared helper 属于高风险路径，除非任务明确要求，否则不要顺手重构。
- **隐式放宽标准**：不要降低 `flag_mask`、跳过 manual/corpus、删除 replay 文件来让测试通过。
- **把诊断快照当 replay 输入**：generated replay 的事实来源仍是 `case_selector + seed_index`，不是 `image_hex`、`initial_state` 或 `result_state`。
- **忽略 feature gate**：不要假设 GitHub hosted runner 支持 AVX2、AVX-512、AES、SHA、BMI、FMA、CET、XSAVE 等可选特性。
- **承诺异步工作**：不能说稍后会跑、后台会做；只能报告当前已真实完成的工作。

## 2. 当前项目架构速记

仓库是 Visual Studio / MSVC C++ 项目，核心产物是静态库。

```text
cpueaxh.sln
├─ cpueaxh      用户态核心静态库
├─ kcpueaxh     内核态静态库变体
├─ example      用户态示例
├─ kexample     KMDF / kernel 示例
└─ test         回归测试可执行程序 test.exe
```

关键目录：

```text
cpueaxh/cpueaxh.hpp          public C ABI
cpueaxh/cpueaxh.cpp          engine、API、hook、escape、执行入口
cpueaxh/cpueaxh_internal.hpp 内部聚合头
cpueaxh/cpueaxh_platform.hpp 用户态/内核态平台抽象
cpueaxh/cpu/                 CPU context、decoder、executor、memory access、inst cache、helper
cpueaxh/memory/manager.hpp   guest/host memory manager、page perms、patch overlay、code_version
cpueaxh/instructions/        各指令和指令族实现
test/framework/              测试框架主体
test/regression/             generated replay corpus
test/manual/                 manual / unsafe-native replay records
docs/                        开发契约、状态表、replay schema、gate 定义
tools/                       CI 和本地验证脚本
.github/workflows/           GitHub Actions 门禁
```

核心执行链路：

```text
cpueaxh_open
→ cpueaxh_set_memory_mode
→ cpueaxh_mem_map / cpueaxh_mem_write / cpueaxh_context_write
→ cpueaxh_emu_start
→ executor fetch/decode
→ decoder 生成 DecodedInst
→ inst_cache 按 RIP + code_version + CPU mode key 缓存
→ dispatch 到 cpueaxh/instructions/*.hpp handler
→ memory manager 做权限、映射、patch、异常、hook
```

## 3. 源文件阅读优先级

不同任务的第一轮阅读建议如下。

### 3.1 新增或修复一条普通用户态安全指令

先读：

```text
docs/instruction-status.json
docs/instruction-test-generator-template.md
test/framework/generated_specs.hpp
test/framework/code_builder.hpp
cpueaxh/cpu/decoder.hpp
cpueaxh/cpu/executor.hpp
cpueaxh/instructions/<相邻指令>.hpp
cpueaxh/instructions/all_instructions.hpp
```

再搜：

```powershell
Get-ChildItem -Recurse -File |
  Where-Object { $_.FullName -notmatch '\\.git\\|node_modules|dist|build|coverage|__pycache__|\.venv' } |
  Select-String -Pattern '<mnemonic>|<case_selector>|<opcode>' -Context 2,2
```

### 3.2 修复 generated differential mismatch

先读：

```text
failure.json 或 failure-bundle/failure.json
docs/replay-schema.md
test/framework/types.hpp
test/framework/runner.hpp
test/framework/generated_specs.hpp
对应 instruction implementation
```

优先用 replay 复现：

```powershell
.\x64\Release\test.exe --replay path\to\failure.json --record-bundle failure-bundle
```

或用 exact selector：

```powershell
.\x64\Release\test.exe --case <exact-case-selector> --seed-index <n> --record-bundle failure-bundle
```

### 3.3 修改 decoder / executor / memory / flags / shared helper

这是高风险任务。先读：

```text
docs/stage3-regression-gates.json
docs/development-workflow.md
TEST_FRAMEWORK_PLAN_CN.md
cpueaxh/cpu/decoder.hpp
cpueaxh/cpu/executor.hpp
cpueaxh/cpu/dispatch_helpers.hpp
cpueaxh/cpu/calc.hpp
cpueaxh/cpu/memory.hpp
cpueaxh/instructions/flags.hpp
test/framework/manual_registry.hpp
test/framework/runner.hpp
```

必须审查 stage3 gate：

```powershell
.\x64\Release\test.exe --list-gates
.\tools\validate-regression-contract.ps1
```

### 3.4 修改测试框架或契约文档

先读：

```text
TEST_FRAMEWORK_PLAN_CN.md
docs/development-workflow.md
docs/replay-schema.md
docs/instruction-test-generator-template.md
docs/stage3-regression-gates.json
tools/validate-regression-contract.ps1
tools/validate-generated-spec-manifest.ps1
tools/validate-instruction-status.ps1
.github/workflows/msvc-test.yml
.github/workflows/extended-regression.yml
```

## 4. 测试架构核心模型

本仓库的测试不是普通单元测试，而是 CPU emulator 的机器码级差分回归框架。

### 4.1 test.exe 入口

`test/main.cpp` 只负责调用：

```cpp
return cpueaxh_test::run_cli(argc, argv);
```

CLI、runner、types、case builder 都在 `test/framework/`。

### 4.2 生成式真机差分测试

主力测试层是 generated true-CPU differential tests。

流程：

```text
ProgramSpec
→ deterministic seed
→ build_case 生成机器码 image 和 initial_context
→ native_runner.asm 在真实 CPU 上执行同一段 image
→ cpueaxh guest mode 执行同一段 image
→ 比较 native 与 emu 结果
```

比较对象包括：

```text
GPR
RIP / RSP 归一化结果
RFLAGS 中架构定义的 flag_mask
数据区 memory
部分 SIMD / MXCSR / x87 相关效果
```

重要规则：RFLAGS 不能全量比较。每个 `ProgramSpec` 必须有明确的 `flag_mask`，只比较架构定义输出位。undefined flags 不应被加入 mask。

### 4.3 Harness

`Harness` 负责把同一条 generated case 跑在真实 CPU 和 emulator 上。

核心行为：

```text
VirtualAlloc native code page / native stack
cpueaxh_open
cpueaxh_set_memory_mode(CPUEAXH_MEMORY_MODE_GUEST)
cpueaxh_mem_map guest code / stack
native_runner.asm 执行 native image
cpueaxh_mem_write 写入 guest image
cpueaxh_emu_start 执行 emulator
比较 native_context、emu_context、guest memory
```

因此，generated case 的机器码必须安全、可返回、可重复、可在用户态 native 执行。

### 4.4 Manual / unsafe-native 覆盖

`test.exe --list-manual` 暴露 manual / unsafe-native 覆盖组。当前重要类别包括：

```text
compat32_control_transfer
exception_priority
invalid_prefix_ud
x87_state
host_stack_roundtrip
cached_rmw_recompute
context_api
simd_encoding_edges
port_io_escape
privileged_system
```

这些覆盖 generated differential 不适合直接 native 执行的内容，例如异常优先级、非法 prefix、x87、privileged/system、port I/O 等。

manual replay record 的 schema 是：

```json
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "memory, stack, canonical-address, and UD exception ordering",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle"
}
```

当前 manual replay 是 coverage-group replay，不是 generated single-case replay。不要把它误写成单条指令精确 replay。

### 4.5 Regression corpus

`test/regression/*.json` 存放 generated replay corpus。默认完整回归会自动 replay 所有 JSON 文件。

最小 generated replay record：

```json
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "replay_hint": "test.exe --case add_rr_rax_rbx --seed-index 0"
}
```

关键规则：

- `case_selector` 必须是 exact generated spec name。
- `seed_index` 必须是未加引号的 JSON number。
- unknown field、duplicate field、trailing comma、malformed nested object 都应被拒绝。
- default full suite 在 `test/regression/` 缺失、不可枚举或没有 replay JSON 时应失败。
- 修复 bug 后，尽量把最小 replay record 加入 `test/regression/`。

## 5. 常用 test.exe 命令

列出 generated specs：

```powershell
.\x64\Release\test.exe --list
```

列出 manual / unsafe-native index：

```powershell
.\x64\Release\test.exe --list-manual
```

列出 stage3 regression gates：

```powershell
.\x64\Release\test.exe --list-gates
```

运行一个 exact generated case：

```powershell
.\x64\Release\test.exe --case add_rr_rax_rbx --seed-index 0 --record-bundle failure-bundle
```

按 substring 过滤 generated cases：

```powershell
.\x64\Release\test.exe --filter add_rr --generated-seeds 128 --record-bundle failure-bundle
```

replay generated regression：

```powershell
.\x64\Release\test.exe --replay test\regression\add_rr_rax_rbx_seed0.json --record-bundle failure-bundle
```

replay manual coverage group：

```powershell
.\x64\Release\test.exe --replay test\manual\exception_priority.json --record-bundle failure-bundle
```

dump host feature matrix：

```powershell
.\x64\Release\test.exe --dump-features cpu-features.json
```

dump generated spec manifest：

```powershell
.\x64\Release\test.exe --dump-specs generated-specs.json
```

默认完整回归：

```powershell
.\x64\Release\test.exe --record-bundle failure-bundle
```

long generated fuzz：

```powershell
.\x64\Release\test.exe --generated-seeds 512 --record-bundle failure-bundle
```

## 6. CI 与本地验证

主要 workflow：

```text
.github/workflows/msvc-test.yml          PR / main 门禁
.github/workflows/extended-regression.yml nightly / workflow_dispatch long regression
```

`msvc-test.yml` 大致流程：

```text
validate-regression-contract.ps1
setup-msbuild
validate-cpueaxh-header-smoke.ps1
msbuild cpueaxh.sln /m /p:Configuration=Release /p:Platform=x64 /t:test
validate-build-log-zero-warnings.ps1
定位 test.exe
dump cpu-features.json
test.exe --list
test.exe --dump-specs generated-specs.json
validate-generated-spec-manifest.ps1
validate-instruction-status.ps1
test.exe --list-manual
test.exe --list-gates
validate-stage3-gate-output.ps1
manual replay sample
validate-strict-replay.ps1
required coverage gates
test.exe --record-bundle failure-bundle
upload evidence / diagnostics
```

CI 证据重点：

```text
cpu-features.json
generated-specs.json
test-specs.log
manual-index.log
stage3-gates.log
manual-replay.log
strict-replay.log
require-coverage.log
test-run.log
build.log
cpu-info.txt
```

本地环境没有 MSBuild 或无法运行 Windows x64 test.exe 时，至少做可用的文档/schema/搜索级验证，并在 PR 中如实说明未运行 build/test。

## 7. 修改指令实现的标准流程

### 7.1 选择任务

从 `docs/instruction-status.json` 选择一个小 form 或明确 encoding group。不要把同一 mnemonic 的所有 forms 默认视为已覆盖或可一次补齐。

优先选择：

```text
todo
implemented_partial
缺少 generated_differential coverage 的小 form
已有 regression failure 的 exact selector
```

### 7.2 补测试或定位现有测试

用户态安全、硬件支持、可 deterministic native 执行的行为，优先进入 generated differential。

需要更新的点通常包括：

```text
test/framework/generated_specs.hpp
test/framework/code_builder.hpp
docs/instruction-status.json
```

如果 bug 来自 generated mismatch，保留或新增：

```text
test/regression/<case_selector>_seed<n>.json
```

### 7.3 实现最小代码改动

常见改动点：

```text
cpueaxh/instructions/<mnemonic>.hpp
cpueaxh/instructions/all_instructions.hpp
cpueaxh/cpu/decoder.hpp
cpueaxh/cpu/dispatch_helpers.hpp
```

优先模仿相邻指令，不要引入全新抽象，除非确实能减少局部重复且不扩大行为面。

### 7.4 验证

最小验证顺序：

```powershell
msbuild cpueaxh.sln /m /p:Configuration=Release /p:Platform=x64 /t:test
.\x64\Release\test.exe --case <exact-name> --seed-index <n> --record-bundle failure-bundle
.\x64\Release\test.exe --record-bundle failure-bundle
```

涉及 status / manifest / regression corpus：

```powershell
.\x64\Release\test.exe --dump-specs generated-specs.json
.\tools\validate-generated-spec-manifest.ps1 -ManifestPath generated-specs.json
.\tools\validate-instruction-status.ps1 -ManifestPath generated-specs.json
.\tools\validate-regression-contract.ps1
```

涉及 replay parser：

```powershell
.\tools\validate-strict-replay.ps1 -TestExe .\x64\Release\test.exe
```

涉及 stage3 gates：

```powershell
.\x64\Release\test.exe --list-gates
.\tools\validate-stage3-gate-output.ps1 -OutputPath stage3-gates.log
```

## 8. 高风险文件与额外要求

以下文件改动风险高，因为可能影响大量旧指令：

```text
cpueaxh/cpu/decoder.hpp
cpueaxh/cpu/executor.hpp
cpueaxh/cpu/dispatch_helpers.hpp
cpueaxh/cpu/calc.hpp
cpueaxh/cpu/memory.hpp
cpueaxh/instructions/flags.hpp
cpueaxh/memory/manager.hpp
cpueaxh/cpueaxh.cpp
```

触碰这些文件时，PR 说明必须包含：

```text
改动原因
影响面
为什么不是更小改动
跑过哪些 targeted tests
是否跑过 full regression
stage3 gate 是否检查
未验证项
```

## 9. 不适合 generated native differential 的情况

以下情况不要直接放进 generated native run：

```text
privileged instruction
port I/O
MSR
VMX/SVM
kernel-state-dependent behavior
可能阻塞或不可恢复的指令
依赖不可控 host 状态的 nondeterministic 行为
当前 runner feature 不保证存在的指令
异常优先级 / split access / canonical address 等复杂 fault ordering
```

处理方式：

```text
在 docs/instruction-status.json 标注 unsafe_for_native 或 manual/model 原因
加入 manual coverage group 或扩展已有 manual case
必要时设计 controlled-runner 策略，但不要假装 hosted CI 已覆盖
```

## 10. PR 内容模板

每个 PR 最终说明至少包含：

```text
Summary:
- 改了什么
- 为什么改

Instruction / test status:
- 是否改变指令行为
- docs/instruction-status.json 是否更新
- generated differential / manual / regression replay 覆盖情况

Validation:
- 本地 build 命令和结果
- targeted test 命令和结果
- full regression 命令和结果
- schema / manifest / status validator 结果
- CI 最新状态

Risk notes:
- 高风险文件
- feature gate 限制
- 未验证项
```

如果只是文档改动，也要说明未改指令行为、未运行 full regression 的原因。

## 11. 失败处理规则

如果测试失败，不要先改实现。先收集：

```text
failure-bundle/failure.json
failure-bundle/failures.json
failure-bundle/cpu-features.json
generated-specs.json
test-run.log
manual-replay.log
strict-replay.log
```

然后判断：

```text
是 generated mismatch？用 --replay 或 --case + --seed-index 复现。
是 schema/parser 失败？先看 docs/replay-schema.md 和 validate-strict-replay.ps1。
是 feature gate 缺失？不要强行要求 unavailable feature；检查 --dump-features。
是 manual case 失败？定位 manual_registry.hpp 中对应 coverage group。
是 CI runner 偶发？先读失败日志；只有明显 runner/cache/network/platform 偶发才重跑。
```

修复后，如果是历史 bug 或 generated mismatch，尽量新增或保留 `test/regression/*.json` replay record。

## 12. 禁止事项清单

绝对不要：

```text
直接改 main
删除或弱化已有测试
删除 regression corpus
降低 flag_mask 来规避失败
把 undefined flags 加入必须比较范围
跳过 manual/corpus 后声称 full regression 通过
把 --filter 当 exact replay selector
把 image_hex 当 replay source of truth
假设 runner 支持所有 CPU features
一次性大改 decoder/executor
修改 workflow 后不说明风险
提交生成目录、缓存目录、依赖目录、二进制或 secret
声称未运行的测试已经通过
```

## 13. 建议的任务启动提示词

让 GPT-5.5 接手具体开发时，可以使用这种约束型提示：

```text
在 qqq694637644/cpueaxh 中完成一个小范围维护任务。
先只读检查当前 main 和相关文档：docs/development-workflow.md、TEST_FRAMEWORK_PLAN_CN.md、docs/instruction-status.json、相关 instruction/test 文件。
不要直接改 main；创建 gpt/* 分支。
只实现 <具体 form / bug / case_selector>，不要重构 decoder/executor，除非为该任务不可避免。
用户态安全行为必须优先补 generated differential 或 replay corpus；unsafe/native 不安全行为必须走 manual/status 标注。
提交前查看 diff，运行最相关测试；无法运行 MSBuild/test.exe 时如实说明。
创建 PR 后查询 CI，最终给 PR、commit、测试和 CI 状态。
```

## 14. 最短接手检查表

开始前：

```text
[ ] 当前分支不是 main，已创建 gpt/* 分支
[ ] 已读相关源码和 docs/development-workflow.md
[ ] 已确认任务是一个小 form / 小 bug / 小测试改动
[ ] 已确认是否需要 generated differential、manual coverage 或 regression replay
```

提交前：

```text
[ ] diff 只包含任务相关文件
[ ] 没有删除或弱化测试
[ ] flag_mask 没有被无理由降低
[ ] docs/instruction-status.json 与实际 coverage 一致
[ ] 已运行 targeted test 或说明无法运行
[ ] 已运行 full regression 或说明无法运行
[ ] 已运行相关 validator 或说明无需运行
```

PR 后：

```text
[ ] 已查询 CI
[ ] 已报告真实 CI 状态
[ ] 风险点和未验证项已写明
```

## 15. 本文档的边界

本文档是给 GPT-5.5 的接手守则，不替代仓库内更具体的契约文件。发生冲突时，以当前仓库源码、测试、CI workflow 和以下文件为准：

```text
TEST_FRAMEWORK_PLAN_CN.md
docs/development-workflow.md
docs/replay-schema.md
docs/instruction-test-generator-template.md
docs/stage3-regression-gates.json
docs/instruction-status.json
.github/pull_request_template.md
```
