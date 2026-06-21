# Cpueaxh

[English](README.md) | [简体中文](README_CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> ![cpueaxh logo](assets/logo.jpg)

`Cpueaxh` is a lightweight, fully dependency-free `x86-64` CPU emulation library with innovative support for emulating execution directly inside the current process memory space.

***This project is under active development. It currently supports common x64 instructions, SSE, SSE2, AVX, AVX2, and a small subset of other instruction sets. The goal is to eventually support the full x86-64 instruction set, including Heaven's Gate transitions. Stay tuned, and issues and contributions are very welcome.***

The project is designed to provide:
- a core execution engine that can be embedded directly in user-mode or kernel/driver-side projects.
- a pure `C ABI` public interface.
- an API style that is as close as practical to [Unicorn](https://github.com/unicorn-engine/unicorn).
- both a [Unicorn](https://github.com/unicorn-engine/unicorn)-like self-managed memory mode **(guest mode)** and direct emulation in real memory space **(host mode)**.
- host-mode memory patching, allowing the emulated CPU to observe bytes different from the underlying real memory.
- a single core codebase that can be built for user mode or compiled directly into a kernel driver through a small platform abstraction layer, without maintaining two copies of the emulator core.
- the library does not depend on exception handling for execution.

### User-mode host-mode demo

![user-mode host-mode demo](assets/example.png)

### Kernel-mode host-mode demo

![kernel-mode host-mode demo](assets/kexample.png)

## Highlights

### 1. Pure C ABI
The public header is [cpueaxh/cpueaxh.hpp](cpueaxh/cpueaxh.hpp).

All exported APIs use the `cpueaxh_*` naming convention and can be called directly from both C and C++.

### 2. Static library form
The core project [cpueaxh/cpueaxh.vcxproj](cpueaxh/cpueaxh.vcxproj) builds as a static library, making integration straightforward.

### 3. Two memory modes
- `CPUEAXH_MEMORY_MODE_GUEST`
  - memory must be mapped explicitly by the user.
  - usage is intentionally close to Unicorn.
- `CPUEAXH_MEMORY_MODE_HOST`
  - emulation runs against the current process address space.
  - supports patch overlays so the CPU can see patched bytes without modifying the underlying real memory.
  - useful for host tracing, live function analysis, and instrumentation.

### 4. Status-based CPU exceptions
Exception state is stored in the CPU context and can be queried through:
- `cpueaxh_code_exception()`
- `cpueaxh_error_code_exception()`

Currently modeled exception types include:
- `#DE`
- `#GP`
- `#SS`
- `#NP`
- `#UD`
- `#PF`
- `#AC`

### 5. Unicorn-like memory management semantics
Guest-mode memory management supports:
- `cpueaxh_mem_map()`
- `cpueaxh_mem_map_ptr()`
- `cpueaxh_mem_unmap()`
- `cpueaxh_mem_protect()`
- `cpueaxh_mem_regions()`

The current implementation includes:
- 4 KB page alignment checks
- range-splitting `unmap` and `protect`
- mapped region enumeration
- separate `READ/WRITE/FETCH_UNMAPPED` results
- separate `READ/WRITE/FETCH_PROT` results

### 6. CPU-level memory attributes
In addition to normal `R/W/X` page permissions, the engine supports CPU-visible page attributes:
- `cpueaxh_mem_set_cpu_attrs()`
- `CPUEAXH_MEM_ATTR_USER`

This allows the emulator to raise proper page faults when CPL and page attributes do not match, rather than degrading to a generic access failure.

### 7. Hooking and escape mechanism
The engine supports basic code hooks, Unicorn-like memory access hooks, and instruction escape handling:
- `cpueaxh_hook_add()` / `cpueaxh_hook_del()`
- `cpueaxh_hook_add_address()` for exact-address hooks
- `CPUEAXH_HOOK_CODE_PRE` and `CPUEAXH_HOOK_CODE_POST`
- `CPUEAXH_HOOK_MEM_READ`, `CPUEAXH_HOOK_MEM_WRITE`, and `CPUEAXH_HOOK_MEM_FETCH`
- `CPUEAXH_HOOK_MEM_READ_UNMAPPED`, `CPUEAXH_HOOK_MEM_WRITE_UNMAPPED`, and `CPUEAXH_HOOK_MEM_FETCH_UNMAPPED`
- `CPUEAXH_HOOK_MEM_READ_PROT`, `CPUEAXH_HOOK_MEM_WRITE_PROT`, and `CPUEAXH_HOOK_MEM_FETCH_PROT`
- `cpueaxh_escape_add()` / `cpueaxh_escape_del()`
- `cpueaxh_host_call()` for bridging an escape into native execution

Memory-access hooks are range-filterable and follow Unicorn-style split semantics. Successful accesses use `cpueaxh_cb_hookmem_t` and report instruction fetches, memory reads, and memory writes together with address, access size, and value. Invalid accesses use `cpueaxh_cb_hookmem_invalid_t`; the callback may fix the mapping/protection state and return nonzero to retry the access, or return `0` to let emulation fail with the corresponding memory error. For invalid reads and fetches the reported value is `0`, while invalid writes report the attempted write value.

An escape is intended for handing selected instructions off from the emulator to a real host-side execution path.
The escape callback receives the decoded instruction bytes together with a mutable `cpueaxh_x86_context`, and may either emulate the instruction manually in `C/C++` or transfer execution through `cpueaxh_host_call()`.
When `cpueaxh_host_call()` is used, the library applies the emulated register, flag, and SIMD state to the real CPU context, jumps into the user-provided native bridge routine, and then captures the resulting machine state back into the emulator context.
In the native bridge routine, the top of the guest stack is prepared with a resume target, so the routine can jump back through the address stored at `[rsp]` when native handling is complete.
This makes escape callbacks suitable both for handwritten `asm` bridges and for higher-level software emulation logic.
The current example project demonstrates both styles: native `asm` bridges for `syscall`, `cpuid`, and `xgetbv`, and direct `C/C++` emulation for instructions such as `rdtsc`, `rdtscp`, and `rdrand`.

The escape mechanism currently supports dispatch for these instruction classes:
- `syscall`, `sysenter`
- `int`, `int1`, `int3`, `ud2`, `hlt`
- `cpuid`, `xgetbv`, `rdtsc`, `rdtscp`, `rdrand`
- `rdsspd`, `rdsspq`
- port I/O instructions `in` and `out`

Escape callbacks are registered per instruction class, can be constrained to an address range, and the current implementation allows one registered escape per instruction class.

### 8. Full public x86 context structure
The public `cpueaxh_x86_context` includes:
- GPRs
- `RIP`
- `RFLAGS`
- `XMM` and upper `YMM` state
- `MMX`
- `MXCSR`
- segment registers
- `GDTR/LDTR`
- `CPL`
- current exception state

This is useful for host mode, escape callbacks, debugging, and state synchronization.

## What is implemented

### Core execution framework
Implemented components include:
- instruction fetch, decode, and dispatch
- general-purpose register read/write
- basic segment and privilege state handling
- memory read/write/execute permission checks
- page-fault generation and page-fault error codes
- hook and escape dispatch

### Major implemented instruction groups
The project already contains a large set of commonly used integer, control-flow, string, bit-manipulation, SSE, AVX, and AVX2 instructions under [cpueaxh/instructions](cpueaxh/instructions).

Major coverage includes:

- Data movement
  - `mov`
  - `movsx`
  - `movzx`
  - `movsxd`
  - `lea`
  - `xchg`
  - `xadd`
  - `cmpxchg`
  - `cmpxchg8b/16b`

- Integer arithmetic and logic
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

- Shifts and bit test operations
  - `shl`
  - `shr`
  - `sar`
  - `rol`
  - `ror`
  - `bt`
  - `bsf`
  - `bswap`

- Control flow
  - `jmp`
  - `jcc`
  - `call`
  - `ret`
  - `setcc`
  - `cmovcc`

- Stack and flags
  - `push`
  - `pop`
  - `pushf`
  - `lahf`
  - `enter`
  - `leave`

- String instructions
  - `movs`
  - `stos`
  - `lods`
  - `cmps`
  - `scas`
  - `rep`

- System, timing, and processor information
  - `cpuid`
  - `rdtsc`
  - `rdtscp`
  - `xgetbv`
  - `rdrand`

- SSE / SSE2 / AVX / partial AVX2
  - `movups`
  - `movaps`
  - `movss`
  - `movd`
  - `movq`
  - `movdq`
  - `pshufd`
  - `vinsertf128`
  - multiple `sse_*` groups
  - multiple `sse2_*` groups
  - `punpcklbw`
  - `punpcklwd`
  - `punpckldq`
  - `punpcklqdq`
  - `pcmpistrm`
  - `pcmpistri`
  - `pcmpestrm`
  - `pcmpestri`
  - multiple VEX-encoded `AVX` groups
  - part of `AVX2`

## Repository layout

- [cpueaxh](cpueaxh)
  - core static library
- [cpueaxh/cpu](cpueaxh/cpu)
  - CPU context, executor, memory access, and helpers
- [cpueaxh/memory](cpueaxh/memory)
  - virtual memory manager
- [cpueaxh/instructions](cpueaxh/instructions)
  - instruction and instruction-family implementations
- [example](example)
  - example program
- [kcpueaxh](kcpueaxh)
  - kernel-mode static library variant of the core
- [kexample](kexample)
  - KMDF kernel usage sample that links against `kcpueaxh`
- [cpueaxh.sln](cpueaxh.sln)
  - Visual Studio solution

## Public API overview

### Engine lifecycle
- `cpueaxh_open()`
- `cpueaxh_close()`

### Memory mode
- `cpueaxh_set_memory_mode()`

### Memory management
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

### Register access
- `cpueaxh_reg_read()`
- `cpueaxh_reg_write()`

### Execution control
- `cpueaxh_emu_start()`
- `cpueaxh_emu_start_function()`
- `cpueaxh_emu_stop()`

### Hooking / escape
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

### Exception query
- `cpueaxh_code_exception()`
- `cpueaxh_error_code_exception()`

## Quick example

### 1. Create an engine
```c
cpueaxh_engine* engine = NULL;
cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
```

### 2. Map a code page in guest mode
```c
cpueaxh_set_memory_mode(engine, CPUEAXH_MEMORY_MODE_GUEST);
cpueaxh_mem_map(engine, 0x1000, 0x1000, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE | CPUEAXH_PROT_EXEC);
cpueaxh_mem_write(engine, 0x1000, code, code_size);
```

### 3. Set registers and start execution
```c
uint64_t rip = 0x1000;
cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RIP, &rip);
cpueaxh_emu_start(engine, 0x1000, 0, 0, 0);
```

### 4. Query mapped regions
```c
cpueaxh_mem_region* regions = NULL;
uint32_t count = 0;
if (cpueaxh_mem_regions(engine, &regions, &count) == CPUEAXH_ERR_OK) {
    cpueaxh_free(regions);
}
```

## Example program

The example is located in [example/main.cpp](example/main.cpp) and includes:
- a basic guest-mode execution demo
- a guest pre-hook demo that prints the current address and the next 16 bytes
- a guest post-hook demo that prints the post-instruction `RIP`
- a guest exact-address hook demo
- a guest memory-access hook demo for fetch/read/write events
- a guest invalid-memory recovery demo for `READ_UNMAPPED` and `WRITE_PROT`
- a host-mode `MessageBoxA` execution demo
- a host-mode memory patch demo
- default escape handlers covering `syscall`, `sysenter`, `int`, `int1`, `int3`, `ud2`, `cpuid`, `xgetbv`, `rdtsc`, `rdtscp`, `rdrand`, `rdsspd`, `rdsspq`, `hlt`, `in`, and `out`

The example project currently has no external library dependency beyond the Windows / MSVC toolchain and uses MASM for the native escape bridge samples.

The kernel sample is located in [kexample/main.cpp](kexample/main.cpp). It demonstrates host-mode execution inside a KMDF non-PnP driver and links against the kernel-mode static library [kcpueaxh](kcpueaxh), so the emulator implementation remains shared between user mode and kernel mode.

## Build

Recommended environment:
- Windows
- Visual Studio 2022
- MSVC v143
- x64
- WDK / KMDF if you want to build [kexample](kexample)

Open [cpueaxh.sln](cpueaxh.sln) and build the solution.

## License

This project is licensed under the [MIT License](LICENSE).

## Current project position

This project currently fits best as a:
- research-oriented or experimental `x86-64` emulator
- Windows host analysis and instrumentation core
- lightweight execution engine with Unicorn-like API design

It is not a complete Unicorn replacement and not a full system virtualization solution, but it already provides substantial user-mode execution, memory permission control, exception modeling, and host escape capabilities.

## Acknowledgements

- [QingChan](https://github.com/QingChan0o0): for major contributions to instruction-set coverage
- [Unicorn](https://github.com/unicorn-engine/unicorn)
- [mwemu](https://github.com/sha0coder/mwemu)

## Community

- QQ Group: 878316370
- Discord: [Join](https://discord.com/invite/U9AgvZkxQm)
