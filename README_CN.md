# Cpueaxh

[English](README.md) | [简体中文](README_CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> ![cpueaxh logo](assets/logo.jpg)

`Cpueaxh` 是一个轻量、完全无依赖的 `x86-64` CPU 模拟库，支持直接在当前进程真实内存空间中进行执行模拟。

***该项目仍在积极开发中。目前已支持 x64 下的常规指令、SSE、SSE2、AVX、AVX2，以及少量其他指令集。项目目标是最终支持完整的 x86-64 指令集，并支持 Heaven's Gate 跳转。敬请期待，也欢迎提交 issue 和贡献代码。***

该项目旨在提供：
- 可直接嵌入用户态或内核/驱动侧项目的核心执行引擎。
- 纯 `C ABI` 公共接口。
- 尽可能接近 [Unicorn](https://github.com/unicorn-engine/unicorn) 的 API 风格。
- 同时支持类似 [Unicorn](https://github.com/unicorn-engine/unicorn) 的自管理内存模式 **(guest mode)** 与真实内存空间直接模拟 **(host mode)**。
- 在 host 模式下支持内存补丁覆盖，使模拟 CPU 看到的字节可与底层真实内存不同。
- 通过一层很小的平台适配封装，让同一份核心代码既可用于用户态，也可直接编译进内核驱动，而不需要维护两套核心实现。
- 执行过程不依赖异常处理机制。

### 用户态 host 模式演示

![用户态 host 模式演示](assets/example.png)

### 内核态 host 模式演示

![内核态 host 模式演示](assets/kexample.png)

## 特性概览

### 1. 纯 C ABI
公共头文件位于 [cpueaxh/cpueaxh.hpp](cpueaxh/cpueaxh.hpp)。

所有导出 API 均使用 `cpueaxh_*` 命名，可直接从 C 和 C++ 调用。

### 2. 静态库形式
核心项目 [cpueaxh/cpueaxh.vcxproj](cpueaxh/cpueaxh.vcxproj) 以静态库形式构建，便于集成。

### 3. 两种内存模式
- `CPUEAXH_MEMORY_MODE_GUEST`
  - 需要由用户显式映射内存。
  - 使用方式有意设计为接近 Unicorn。
- `CPUEAXH_MEMORY_MODE_HOST`
  - 直接针对当前进程地址空间执行模拟。
  - 支持补丁覆盖，使 CPU 可看到未实际写回到底层真实内存的补丁字节。
  - 适用于主机侧跟踪、在线函数分析与插桩。

### 4. 基于状态的 CPU 异常
异常状态保存在 CPU 上下文中，可通过以下接口查询：
- `cpueaxh_code_exception()`
- `cpueaxh_error_code_exception()`

当前已建模的异常类型包括：
- `#DE`
- `#GP`
- `#SS`
- `#NP`
- `#UD`
- `#PF`
- `#AC`

### 5. 类 Unicorn 的内存管理语义
Guest 模式内存管理支持：
- `cpueaxh_mem_map()`
- `cpueaxh_mem_map_ptr()`
- `cpueaxh_mem_unmap()`
- `cpueaxh_mem_protect()`
- `cpueaxh_mem_regions()`

当前实现包括：
- 4 KB 页对齐检查
- 支持按区间拆分的 `unmap` 与 `protect`
- 已映射区域枚举
- 区分 `READ/WRITE/FETCH_UNMAPPED` 结果
- 区分 `READ/WRITE/FETCH_PROT` 结果

### 6. CPU 级内存属性
除普通的 `R/W/X` 页权限外，引擎还支持 CPU 可见的页属性：
- `cpueaxh_mem_set_cpu_attrs()`
- `CPUEAXH_MEM_ATTR_USER`

这样在 CPL 与页属性不匹配时，模拟器可以触发正确的页错误，而不是退化成普通访问失败。

### 7. Hook 与 escape 机制
引擎支持基础代码 Hook、类似 Unicorn 的内存访问 Hook，以及指令 escape 处理：
- `cpueaxh_hook_add()` / `cpueaxh_hook_del()`
- `cpueaxh_hook_add_address()`：精确地址 Hook
- `CPUEAXH_HOOK_CODE_PRE` 与 `CPUEAXH_HOOK_CODE_POST`
- `CPUEAXH_HOOK_MEM_READ`、`CPUEAXH_HOOK_MEM_WRITE` 与 `CPUEAXH_HOOK_MEM_FETCH`
- `CPUEAXH_HOOK_MEM_READ_UNMAPPED`、`CPUEAXH_HOOK_MEM_WRITE_UNMAPPED` 与 `CPUEAXH_HOOK_MEM_FETCH_UNMAPPED`
- `CPUEAXH_HOOK_MEM_READ_PROT`、`CPUEAXH_HOOK_MEM_WRITE_PROT` 与 `CPUEAXH_HOOK_MEM_FETCH_PROT`
- `cpueaxh_escape_add()` / `cpueaxh_escape_del()`
- `cpueaxh_host_call()`：将 escape 桥接到原生执行流

内存访问 Hook 现在按 Unicorn 风格拆分语义。成功访问使用 `cpueaxh_cb_hookmem_t`，可观察指令抓取、内存读取、内存写入的地址、大小和值；未映射/权限 Hook 使用 `cpueaxh_cb_hookmem_invalid_t`，回调中可以修复映射或权限，并返回非 `0` 让引擎重试该访问，返回 `0` 则保持原来的失败路径。非法读取与抓取的 `value` 为 `0`，非法写入的 `value` 为尝试写入的值。

escape 的用途，是把模拟器中的特定指令转交给真实主机侧执行路径处理。
escape 回调会收到已解码的指令字节以及一个可修改的 `cpueaxh_x86_context`，用户既可以在 `C/C++` 中手动模拟该指令，也可以通过 `cpueaxh_host_call()` 转入原生桥接代码。
当使用 `cpueaxh_host_call()` 时，库会先把模拟上下文中的寄存器、标志位和 SIMD 状态应用到真实 CPU 上下文，再跳入用户提供的原生桥接例程，最后把执行结果机器状态回写到模拟器上下文。
在原生桥接例程中，guest 栈顶会被预先布置一个恢复目标地址，因此桥接例程完成后可以通过 `[rsp]` 中保存的地址跳回模拟流程。
因此 escape 回调既适合手写 `asm` 桥，也适合更高层的软模拟逻辑。
当前示例项目同时展示了这两种方式：`syscall`、`cpuid`、`xgetbv` 使用原生 `asm` 桥；`rdtsc`、`rdtscp`、`rdrand` 等则直接在 `C/C++` 中模拟。

当前 escape 机制可分发的指令类别包括：
- `syscall`, `sysenter`
- `int`, `int1`, `int3`, `ud2`, `hlt`
- `cpuid`, `xgetbv`, `rdtsc`, `rdtscp`, `rdrand`
- `rdsspd`, `rdsspq`
- 端口 I/O 指令 `in` 与 `out`

escape 回调按“指令类别”注册，可限制生效地址范围；当前实现中，每种指令类别只允许注册一个 escape。

### 8. 完整公开的 x86 上下文结构
公共 `cpueaxh_x86_context` 包含：
- GPR
- `RIP`
- `RFLAGS`
- `XMM` 与高位 `YMM` 状态
- `MMX`
- `MXCSR`
- 段寄存器
- `GDTR/LDTR`
- `CPL`
- 当前异常状态

这对于 host 模式、escape 回调、调试和状态同步都很有用。

## 当前已实现内容

### 核心执行框架
已实现的组件包括：
- 指令抓取、解码与分发
- 通用寄存器读写
- 基础段与特权级状态处理
- 内存读/写/执行权限检查
- 页错误生成与页错误码处理
- Hook 与 escape 分发

### 主要已实现指令组
项目已经在 [cpueaxh/instructions](cpueaxh/instructions) 下实现了大量常用的整数、控制流、字符串、位操作、SSE、AVX 与 AVX2 指令。

主要覆盖包括：

- 数据传输
  - `mov`
  - `movsx`
  - `movzx`
  - `movsxd`
  - `lea`
  - `xchg`
  - `xadd`
  - `cmpxchg`
  - `cmpxchg8b/16b`

- 整数算术与逻辑
  - `add`
  - `adc`
  - `sub`
  - `sbb`
  - `imul`
  - `mul`
  - `idiv`
  - `div`
  - `inc`
  - `dec`
  - `neg`
  - `not`
  - `and`
  - `or`
  - `xor`
  - `test`
  - `cmp`

- 移位与位测试操作
  - `shl`
  - `shr`
  - `sar`
  - `rol`
  - `ror`
  - `bt`
  - `bsf`
  - `bswap`

- 控制流
  - `jmp`
  - `jcc`
  - `call`
  - `ret`
  - `setcc`
  - `cmovcc`

- 栈与标志位
  - `push`
  - `pop`
  - `pushf`
  - `lahf`
  - `enter`
  - `leave`

- 字符串指令
  - `movs`
  - `stos`
  - `lods`
  - `cmps`
  - `scas`
  - `rep`

- 系统、计时与处理器信息
  - `cpuid`
  - `rdtsc`
  - `rdtscp`
  - `xgetbv`
  - `rdrand`

- SSE / SSE2 / AVX / 部分 AVX2
  - `movups`
  - `movaps`
  - `movss`
  - `movd`
  - `movq`
  - `movdq`
  - `pshufd`
  - `vinsertf128`
  - 多个 `sse_*` 分组
  - 多个 `sse2_*` 分组
  - `punpcklbw`
  - `punpcklwd`
  - `punpckldq`
  - `punpcklqdq`
  - `pcmpistrm`
  - `pcmpistri`
  - `pcmpestrm`
  - `pcmpestri`
  - 多个 VEX 编码 `AVX` 分组
  - 部分 `AVX2`

## 仓库结构

- [cpueaxh](cpueaxh)
  - 核心静态库
- [cpueaxh/cpu](cpueaxh/cpu)
  - CPU 上下文、执行器、内存访问与辅助函数
- [cpueaxh/memory](cpueaxh/memory)
  - 虚拟内存管理器
- [cpueaxh/instructions](cpueaxh/instructions)
  - 指令及指令族实现
- [example](example)
  - 示例程序
- [kcpueaxh](kcpueaxh)
  - 内核态核心静态库
- [kexample](kexample)
  - KMDF 内核使用示例，链接 `kcpueaxh`
- [cpueaxh.sln](cpueaxh.sln)
  - Visual Studio 解决方案

## 公共 API 概览

### 引擎生命周期
- `cpueaxh_open()`
- `cpueaxh_close()`

### 内存模式
- `cpueaxh_set_memory_mode()`

### 内存管理
- `cpueaxh_mem_map()`
- `cpueaxh_mem_map_ptr()`
- `cpueaxh_mem_unmap()`
- `cpueaxh_mem_protect()`
- `cpueaxh_mem_set_cpu_attrs()`
- `cpueaxh_mem_regions()`
- `cpueaxh_mem_read()`
- `cpueaxh_mem_write()`
- `cpueaxh_mem_patch_add()`
- `cpueaxh_mem_patch_del()`
- `cpueaxh_free()`

### 寄存器访问
- `cpueaxh_reg_read()`
- `cpueaxh_reg_write()`

### 执行控制
- `cpueaxh_emu_start()`
- `cpueaxh_emu_start_function()`
- `cpueaxh_emu_stop()`

### Hook / escape
- `cpueaxh_cb_hookcode_t`
- `cpueaxh_cb_hookmem_t`
- `cpueaxh_cb_hookmem_invalid_t`
- `cpueaxh_hook_add()`
- `cpueaxh_hook_add_address()`
- `cpueaxh_hook_del()`
- `CPUEAXH_HOOK_CODE_PRE`
- `CPUEAXH_HOOK_CODE_POST`
- `CPUEAXH_HOOK_MEM_READ`
- `CPUEAXH_HOOK_MEM_WRITE`
- `CPUEAXH_HOOK_MEM_FETCH`
- `CPUEAXH_HOOK_MEM_READ_UNMAPPED`
- `CPUEAXH_HOOK_MEM_WRITE_UNMAPPED`
- `CPUEAXH_HOOK_MEM_FETCH_UNMAPPED`
- `CPUEAXH_HOOK_MEM_READ_PROT`
- `CPUEAXH_HOOK_MEM_WRITE_PROT`
- `CPUEAXH_HOOK_MEM_FETCH_PROT`
- `cpueaxh_escape_add()`
- `cpueaxh_escape_del()`
- `cpueaxh_host_call()`

### 异常查询
- `cpueaxh_code_exception()`
- `cpueaxh_error_code_exception()`

## 快速示例

### 1. 创建引擎
```c
cpueaxh_engine* engine = NULL;
cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
```

### 2. 在 guest 模式映射代码页
```c
cpueaxh_set_memory_mode(engine, CPUEAXH_MEMORY_MODE_GUEST);
cpueaxh_mem_map(engine, 0x1000, 0x1000, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE | CPUEAXH_PROT_EXEC);
cpueaxh_mem_write(engine, 0x1000, code, code_size);
```

### 3. 设置寄存器并开始执行
```c
uint64_t rip = 0x1000;
cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RIP, &rip);
cpueaxh_emu_start(engine, 0x1000, 0, 0, 0);
```

### 4. 查询映射区域
```c
cpueaxh_mem_region* regions = NULL;
uint32_t count = 0;
if (cpueaxh_mem_regions(engine, &regions, &count) == CPUEAXH_ERR_OK) {
    cpueaxh_free(regions);
}
```

## 示例程序

示例位于 [example/main.cpp](example/main.cpp)，当前包括：
- 基础 guest 模式执行示例
- guest pre-hook 示例：打印当前地址和后续 16 字节
- guest post-hook 示例：打印指令执行后的 `RIP`
- guest 精确地址 hook 示例
- guest 内存访问 hook 示例：观察 fetch/read/write
- guest 非法内存恢复示例：恢复 `READ_UNMAPPED` 与 `WRITE_PROT`
- host 模式 `MessageBoxA` 执行示例
- host 模式内存补丁示例
- 默认 escape 处理器，覆盖 `syscall`、`sysenter`、`int`、`int1`、`int3`、`ud2`、`cpuid`、`xgetbv`、`rdtsc`、`rdtscp`、`rdrand`、`rdsspd`、`rdsspq`、`hlt`、`in`、`out`

当前示例项目除 Windows / MSVC 工具链外不依赖其他外部库，并使用 MASM 展示原生 escape 桥接样例。

内核示例位于 [kexample/main.cpp](kexample/main.cpp)，展示了如何在 KMDF 非 PnP 驱动中运行 host 模式示例，并通过内核态静态库 [kcpueaxh](kcpueaxh) 复用同一个模拟器核心实现。

## 构建

推荐环境：
- Windows
- Visual Studio 2022
- MSVC v143
- x64
- 如果需要构建 [kexample](kexample)，还需要安装 WDK / KMDF

打开 [cpueaxh.sln](cpueaxh.sln) 并构建解决方案。

## 开源协议

本项目采用 [MIT License](LICENSE) 开源。

## 当前项目定位

该项目当前更适合作为：
- 面向研究或实验的 `x86-64` 模拟器
- Windows 主机侧分析与插桩核心
- 具有 Unicorn 风格 API 设计的轻量执行引擎

它还不是完整的 Unicorn 替代品，也不是完整的系统虚拟化方案，但目前已经具备较完整的用户态执行、内存权限控制、异常建模与 host escape 能力。

## 致谢

- [QingChan](https://github.com/QingChan0o0)：感谢其对指令集补全的大量贡献
- [Unicorn](https://github.com/unicorn-engine/unicorn)
- [mwemu](https://github.com/sha0coder/mwemu)

## 项目交流方式

- QQ 群：878316370
- Discord：[Join](https://discord.com/invite/U9AgvZkxQm)
