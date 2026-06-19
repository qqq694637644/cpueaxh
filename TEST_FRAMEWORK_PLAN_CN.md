# Cpueaxh 自动化指令开发与回归验证框架设计计划

## 1. 背景与目标

`Cpueaxh` 的长期目标是补全 x86-64 / AMD64 指令集模拟。随着支持的指令越来越多，最大的风险不是“新指令写得慢”，而是“新指令或公共 helper 改动破坏旧指令”。因此，后续开发必须先建立一套以旧指令可靠性为核心的验证框架。

本计划的核心目标是：

1. 任何新指令开发都不能降低旧指令可靠性。
2. 用户态可安全执行且当前硬件支持的指令改动，必须优先通过真机差分测试验证；不适合真机执行的指令必须有 manual/model/受控环境测试，并在状态表中标注原因。
3. 每个失败都应尽量留下可复现记录。
4. 每个历史 bug 都应转化为长期回归用例。
5. GPT 或其他 AI 只能在验证门禁内开发，不能绕过测试、删除测试或扩大无审查改动范围。

## 2. 总体原则

### 2.1 旧指令优先保护

后续所有开发流程都应遵循：

```text
实现新指令
→ 跑新指令定向测试
→ 跑全部旧指令回归测试
→ 跑手写历史/边界回归测试
→ CI 真机验证通过
→ 人工 review
→ 合并
```

禁止只跑新指令测试后直接合入。

### 2.2 小批量指令开发

每个 PR 只允许覆盖一个小指令族或一个明确编码族。例如：

```text
允许：实现 BT r/m64, imm8 的一个变体
允许：补全 MOVZX 的一个未覆盖编码
不建议：一次性重构 decoder 并补一大批 SSE/AVX 指令
```

越靠近公共路径的修改，PR 越要小。

高风险公共路径包括：

```text
cpueaxh/cpu/decoder.hpp
cpueaxh/cpu/executor.hpp
cpueaxh/cpu/dispatch_helpers.hpp
cpueaxh/cpu/calc.hpp
cpueaxh/cpu/memory.hpp
cpueaxh/instructions/flags.hpp
```

这些文件一旦改错，可能影响大量旧指令，必须重点审查。

### 2.3 真机差分作为主要判定标准

用户态可安全执行的普通指令，应尽量通过以下方式验证：

```text
同一段机器码
同一初始寄存器 / flags / SIMD / 内存状态
先在真实 CPU 上执行
再在 cpueaxh 模拟器中执行
比较执行后的寄存器、RFLAGS、SIMD、MXCSR、内存和异常结果
```

当前 `test/demo/framework.hpp` 已经具备这种基础能力，应继续扩展，而不是另起一套完全无关的测试系统。

## 3. 测试分层设计

### 3.1 生成式真机差分测试

这是主力测试层。

每个 `ProgramSpec` 描述一类可生成的测试程序，包括：

```text
family: 指令类别
op: 指令操作
variant: 编码或寄存器/内存变体
flag_mask: 需要比较的 RFLAGS 位
name: 可过滤、可定位的测试名称
```

测试流程：

```text
seed-index
→ 计算 deterministic seed
→ 生成机器码 image
→ 构造初始上下文
→ 真机运行 native runner
→ cpueaxh guest 模式运行
→ 比较结果
```

比较内容至少包括：

```text
RAX-R15
RIP / RSP 归一化后结果
RFLAGS 中该指令定义的 flag mask
内存数据区
必要时比较 XMM/YMM/ZMM/K/MXCSR/x87 状态
```

注意：RFLAGS 不能简单全量比较。部分指令存在 undefined flags，只应比较架构定义的 flag mask。

第一阶段的生成式差分主要比较 GPR、masked RFLAGS 和数据区。当前很多 SIMD 结果是通过 store 到数据区后比较，并不等同于已经通用地直接比较完整 YMM/ZMM/K/x87 register file；完整扩展状态直接比较属于后续扩展项。

### 3.2 手写特殊回归测试

生成式测试无法覆盖所有边界。例如：

```text
非 canonical 地址异常
stack unmapped 异常
LOCK 前缀非法组合
VEX/EVEX 非法编码
兼容模式控制转移
host stack roundtrip
缓存重解码路径
x87 控制/状态特殊行为
```

这些应保留为手写测试，并在全量回归中始终运行。

### 3.3 历史 bug 回归语料库

每次修 bug 后，应把复现数据沉淀到：

```text
test/regression/
```

初期可以只保存 JSON/Markdown 记录；后续应逐步支持自动 replay。

目标是让 bug 修复后不可回退：

```text
发现 bug
→ 保存 failure.json
→ 修复
→ 将复现最小化
→ 加入 regression corpus
→ 后续每个 PR 自动跑
```

### 3.4 CI 必跑回归

涉及指令实现、测试框架、构建配置、workflow 或 `docs/instruction-status.yml` 的 PR 必跑。普通 README/说明文档改动不应自动触发完整回归，以避免无关提交消耗 runner：

```text
Windows x64 Release build
生成式真机差分测试
手写特殊回归测试
失败记录 artifact 上传
```

第一阶段 workflow 使用 `paths` 过滤触发范围：`.github/workflows/**`、`.github/pull_request_template.md`、`cpueaxh/**`、`test/**`、`TEST_FRAMEWORK_PLAN_CN.md`、开发契约文档、`docs/instruction-status.yml`、`cpueaxh.sln`、`*.vcxproj`、`*.props`、`*.targets` 相关改动才触发。

GitHub hosted runner 可扩展：

```text
更多 seed
记录实际 CPU feature 矩阵
更长时间 fuzz
AVX2 / AVX-512 / AES / SHA / BMI / FMA / CET 等特性专项
```

## 4. 测试命令设计

当前第一版已经给 `test.exe` 增加基础控制参数：

```powershell
.\x64\Release\test.exe --list
.\x64\Release\test.exe --list-manual
.\x64\Release\test.exe --list-gates
.\x64\Release\test.exe --manual-case exception_priority
.\x64\Release\test.exe --case add_rr_rax_rbx
.\x64\Release\test.exe --filter-exact add_rr_rax_rbx
.\x64\Release\test.exe --filter add_rr_rax_rbx
.\x64\Release\test.exe --filter add_rr_rax_rbx --seed-index 0
.\x64\Release\test.exe --generated-seeds 512 --record-bundle failure-bundle
.\x64\Release\test.exe --replay test\regression\add_rr_rax_rbx_seed0.json
.\x64\Release\test.exe --replay test\manual\exception_priority.json --record-bundle failure-bundle
.\x64\Release\test.exe --dump-features cpu-features.json
.\x64\Release\test.exe --dump-specs generated-specs.json
.\x64\Release\test.exe --record-bundle failure-bundle
.\x64\Release\test.exe --no-manual
.\x64\Release\test.exe --no-regression-corpus
```

各参数用途：

| 参数 | 用途 |
| --- | --- |
| `--list` | 列出当前生成式差分测试 spec，便于审查和定位 |
| `--list-manual` | 列出 manual/unsafe-native 覆盖索引 |
| `--list-gates` | 列出第三阶段 regression gate 索引 |
| `--manual-case <name>` | 复现一个 manual/unsafe-native 覆盖组；当前会运行完整 manual special suite 以避免误报 |
| `--case <exact-name>` | 精确选择一个生成式差分测试 spec |
| `--filter-exact <exact-name>` | `--case` 的别名 |
| `--filter <substring>` | 只运行名称包含该 substring 的 spec，用于新指令定向开发；不是精确单 case selector |
| `--seed-index <index>` | 只运行一个 deterministic seed，便于复现；默认完整回归使用 0..127，`--generated-seeds` 可扩大范围 |
| `--generated-seeds <count>` | 控制每个生成式 spec 运行的 seed 数，供 long fuzz / nightly 使用 |
| `--replay <path>` | 从 failure/regression JSON 中读取 `case_selector` 和 `seed_index` 并复现 |
| `--dump-features <path>` | 输出测试程序自身识别到的 feature-gated 测试矩阵，schema 为 `cpueaxh.host-features.v1` |
| `--dump-specs <path>` | 输出当前硬件 feature gate 下实际生成的 spec manifest，schema 为 `cpueaxh.generated-specs.v1` |
| `--record-failure <path>` | 将第一个失败写成 JSON |
| `--record-bundle <dir>` | 将 `cpu-features.json` 和失败时的 `failure.json` 写入诊断目录 |
| `--no-manual` | 跳过手写特殊用例，主要用于快速定向调试 |
| `--no-regression-corpus` | 跳过 `test/regression/*.json` replay corpus |

默认无参数时必须运行完整回归：生成式差分、manual special cases，以及 `test/regression/*.json` replay corpus，不应默认进入精简模式。

list 模式不允许吞掉运行参数：`--list-manual` 和 `--list-gates` 必须单独使用；`--list` 只能和 `--case` / `--filter-exact` / `--filter` 组合用于筛选列表，不能和 `--manual-case`、`--seed-index`、`--generated-seeds`、`--record-failure`、`--record-bundle`、`--replay` 或 skip 选项组合。

## 5. 失败记录格式

第一版 `failure.json` 目标字段：

```json
{
  "schema": "cpueaxh.failure.v1",
  "case_name": "add_rr_rax_rbx:123456",
  "detail": "rax guest=0x... native=0x...",
  "spec_name": "add_rr_rax_rbx",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "seed": "123456",
  "image_hex": "48 01 d8 c3",
  "replay_hint": "test.exe --case add_rr_rax_rbx --seed-index 0"
}
```

第二阶段已经将生成式用例 replay 从 substring `--filter` 提升为精确 selector：`failure.json` 使用必填 `case_selector`，要求 `schema` 为 `cpueaxh.failure.v1`，要求 `seed_index` 为未加引号的 JSON number，`replay_hint` 使用 `--case <exact-name> --seed-index <n>`，并支持 `test.exe --replay <path>`。`--seed-index` 不再受默认 seed 总数限制，因此 long fuzz 中失败的单个 seed 可以直接用 replay hint 复现。`cpueaxh.manual-index.v1` 记录提供 manual/unsafe-native 覆盖组的结构化 replay 入口；当前 manual replay 会验证 `case_selector` 存在于 manual index，并运行完整 manual special suite，而不是误称可以精确执行尚未拆分的单条 manual case。`--dump-features`、`--dump-specs` 和 `--record-bundle` 提供最小结构化证据包，记录测试程序自身识别的 feature matrix、当前实际 generated spec manifest 与失败 replay 记录。默认完整回归会在 `test/regression/` 缺失、不可枚举或没有 replay JSON 时失败，避免 corpus 静默消失。

当前 generated failure 已落地的结构化初始状态字段：

```text
initial_regs
initial_rflags
initial_xmm
initial_mxcsr
initial_memory
```

这些字段写入 `initial_state`，schema 为 `cpueaxh.generated-initial-state.v1`，用于 debugging 和最小化。实际 replay 仍必须以 `case_selector + seed_index` 为准，不能把快照当作替代生成器。

当前 generated differential mismatch 已落地的结果快照字段：

```text
native_result
emu_result
```

这些字段写入 `result_state`，schema 为 `cpueaxh.generated-result-state.v1`，包含 native/emu 的 GPR、RIP、RFLAGS、MXCSR 和数据区快照。native 指针值会先规范化为 guest 地址。该快照只用于分析差异，replay 仍以 deterministic selector/seed 机制为准。

当前 failure record 也会内嵌 `host_features`，schema 为 `cpueaxh.host-features.v1`，包含 CPUID vendor、max leaf、leaf7 max subleaf 和测试程序用于 feature gate 的布尔矩阵。它和 `cpu-features.json` 一致，用于解释当前 runner 实际执行了哪些 generated specs。

后续可继续扩展字段：

```text
initial_ymm/zmm/k/x87
host_cpu_model/brand string
```

## 6. 指令状态表

根据信息复杂度，使用：

```text
docs/instruction-status.yml
```

记录指令实现状态、测试覆盖状态、feature gate、已知风险。

状态表必须保守解释：`implemented` 只代表状态表中列明的 form/encoding 已实现并受回归保护；未列明的 operand-size、addressing form、prefix/VEX/EVEX encoding、feature-gated form 不能因为同 mnemonic 标为 `implemented` 就推断为已支持。CI 通过 `tools/validate-instruction-status.ps1` 交叉检查状态表、`generated-specs.json` 和 regression corpus，避免 coverage 声明漂移。

建议状态包括：

```text
todo
implemented
implemented_partial
differential_tested
manual_tested
feature_gated
unsafe_for_native
blocked
```

后续 GPT 自动开发时，只能从状态表中选择明确的 `todo` 或 `implemented_partial` 小项，不能自行假设“某指令已完整支持”。状态表后续应扩展到 mnemonic + form + encoding + operand-size + feature gate 粒度。

## 7. AI / GPT 开发工作流

后续每次让 GPT 开发新指令时，流程应固定为：

```text
1. 从 instruction-status.yml 选择一个小任务
2. 阅读相邻指令实现和 decoder 分发逻辑
3. 先补或扩展测试
4. 运行定向测试，确认原本失败或覆盖缺口存在
5. 实现最小代码改动
6. 运行定向测试
7. 运行完整 test.exe 回归
8. 创建 PR
9. 等 CI 通过
10. 人工 review 后合并
```

AI 不允许做以下事情：

```text
删除已有测试
降低 flag mask 以规避失败
跳过旧指令回归
一次性大范围重构 decoder/executor
在未解释风险的情况下改公共 helper
声称未运行过的测试已经通过
```

## 8. GitHub runner 验证矩阵

GitHub hosted runner 是当前验证门禁目标。测试程序会在运行时通过 CPUID 和 OS-enabled state 检测 feature，并只运行当前 runner 支持的 generated specs。因此不需要引入自托管硬件作为当前 PR 门禁。

每次 hosted CI 都必须保留：

```text
cpu-info.txt
cpu-features.json
generated-specs.json
test-specs.log
manual-index.log
stage3-gates.log
test-run.log
```

每次运行前通过 CPUID 检测 feature，只运行当前 GitHub runner 支持的测试。

可选 feature 包括：

```text
SSE3 / SSSE3 / SSE4.1 / SSE4.2
AES
PCLMULQDQ
AVX / AVX2 / AVX-512
BMI1 / BMI2
FMA
SHA
POPCNT / LZCNT
RDRAND / RDSEED
CET 相关指令
XSAVE/XRSTOR 相关状态
```

## 9. 不适合直接真机执行的指令

部分指令不能简单在普通用户态测试进程中执行，例如：

```text
特权指令
I/O 端口指令
MSR 访问
VMX/SVM 虚拟化指令
部分系统调用路径
页表/段权限相关的完整系统语义
```

这些应分类处理：

```text
1. 纯软件建模：按手册实现，不直接真机执行。
2. escape：由 host callback 或已有 escape 机制处理。
3. 内核/虚拟化专用测试：不纳入当前 GitHub hosted runner 门禁，必须通过 model/escape/manual 方式覆盖。
4. 标记 unsafe_for_native：在状态表中明确不能普通真机差分。
```

## 10. 第一阶段当前 PR 范围

当前第一版测试框架 PR 只做基础设施，不改任何指令实现。

已包含：

```text
.github/workflows/msvc-test.yml
docs/development-workflow.md
docs/instruction-status.yml
test/regression/README.md
test.exe 参数扩展
failure.json 首个失败记录
CI Windows x64 Release 全量回归
```

当前已验证：

```text
GitHub Actions Windows x64 Release CI 已通过
本地后端无 MSVC/MSBuild，无法直接编译运行
未修改 instructions/* 指令实现文件
```

## 11. 第二阶段建议 / 当前实现

第二阶段当前已补齐生成式 replay 和 corpus 自动化的最小闭环：

```text
1. 增加 `--case <exact-name>` 和 `--filter-exact <name>` 精确 selector
2. 让 failure.json replay_hint 使用精确 selector，而不是 substring filter
3. 增加 --replay <path>
4. 将 test/regression/*.json 纳入默认全量回归
5. 增加一个最小 replay corpus 样例
6. 扩展 CI failure artifact：上传 failure.json、测试日志、manual index 和 runner CPU 信息
```

第二阶段仍建议后续继续补：

```text
1. manual/unsafe-native 已有 `cpueaxh.manual-index.v1` 结构化 replay 入口，并在 CI 中用 `test/manual/exception_priority.json` 验证
2. `--record-bundle` 已上传最小 replay bundle：feature matrix + generated spec manifest + failure record + host_features + generated initial_state/result_state + 日志
3. 不引入自托管硬件 workflow；当前以 GitHub hosted runner 的 feature-gated 测试和证据 artifact 为准
```

## 12. 第三阶段建议 / 当前实现

第三阶段开始为后续全量 AMD64 指令补全服务。当前已实现验证框架层面的第三阶段门禁，不等同于已经补完整 AMD64 指令集：

```text
1. instruction-status.yml 已采用 mnemonic + form + encoding + operand-size + feature gate 粒度结构
2. docs/generator-templates.yml 定义每个主要指令族的生成器模板和安全约束
3. extended-regression.yml 提供 nightly / workflow_dispatch long fuzz 入口
4. docs/hardware-runner-matrix.md 文档化 GitHub hosted runner feature matrix 和 unsafe-native 策略
5. docs/stage3-regression-gates.yml 定义 decoder/executor/public helper 专项 gate
6. --list-gates 和 CI stage3-gates.log 暴露 undefined flags、异常优先级、内存访问顺序等 gate
7. tools/validate-regression-contract.ps1 在 CI 中结构化校验状态表、replay corpus、stage3 gate 名称/必填字段和生成器模板 family/必填段契约
8. tools/validate-stage3-gate-output.ps1 在 CI 中校验 `--list-gates` 输出与 docs/stage3-regression-gates.yml 的 gate 名称、category、command 一致
9. --dump-specs 和 tools/validate-generated-spec-manifest.ps1 在 CI 中校验当前 generated spec manifest、唯一 selector，以及 regression corpus selector 均存在于 manifest 中
10. tools/validate-instruction-status.ps1 在 CI 中校验 instruction-status form 必填字段、coverage 声明、generated selector、regression replay 和 unsafe-native 标注
11. 当前不使用自托管硬件；GitHub hosted runner 保留 feature matrix、generated spec manifest、manual replay 和 full regression 证据
```

第三阶段仍未声称完成完整 AMD64 指令集覆盖。后续补具体指令时，必须继续按 instruction-status.yml form 粒度扩展状态表、测试生成器和 regression corpus。

## 13. 合并前审查清单

审查当前 PR 时建议重点看：

```text
1. CI workflow 是否符合仓库策略
2. 默认 test.exe 是否仍然跑完整回归
3. --filter / --seed-index 是否只影响定向调试，不影响默认门禁
4. failure.json 是否使用 exact case_selector，而不是 substring filter
5. --replay 是否能复现 test/regression/*.json 中的生成式用例
6. 手写 special tests 在 CPUEAXH_TEST_CONTINUE=1 下是否仍会最终失败
7. 文档中对 AI 开发边界是否足够明确
```

本计划的核心判断是：先建立“旧指令不可回退”的验证门禁，再逐步让 GPT 开发新指令。没有这套门禁之前，不应开始大规模自动化补全 AMD64 指令集。
