# KytyPS5 Full GuestCpuContext & Runtime Bridge Specification

## Overview
This specification details the **Full GuestCpuContext & Runtime Calling Convention Bridge** in **KytyPS5**. The system provides 16-byte aligned guest CPU state representations (16 GPRs, 16 128-bit XMM registers, 16 256-bit YMM high registers, AVX state active tracking, MXCSR control/status, RFLAGS), Lazy Register Synchronization masks, Fast SysV AMD64 <-> AAPCS64 ARM64 ABI transition trampolines, 16-Byte Stack Alignment Verification, and Exception-Safe Transition Frames.

---

## 1. GuestCpuContext Memory Layout (16-Byte Aligned)

| Field | Type | Offset Alignment | Description |
| :--- | :--- | :--- | :--- |
| `rax..r15` | `uint64_t[16]` | 8-byte | General Purpose Registers (GPRs RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8..R15) |
| `xmm[16][2]` | `alignas(16) uint64_t[16][2]` | 16-byte | Low 128-bit XMM SIMD Registers (XMM0..XMM15) |
| `ymm_hi[16][2]` | `alignas(16) uint64_t[16][2]` | 16-byte | High 128-bit YMM Upper Extensions (YMM0_hi..YMM15_hi) |
| `avx_state_active` | `bool` | 1-byte | Tracks whether upper 128-bit YMM state is dirty/active |
| `mxcsr` | `uint32_t` | 4-byte | IEEE-754 SSE/AVX Control & Status Register (Default: `0x1F80`) |
| `rflags` | `uint64_t` | 8-byte | x86 CPU Status Flags (CF, PF, AF, ZF, SF, OF) |
| `dirty_gpr_mask` | `uint32_t` | 4-byte | Bitmask of dirty GPRs pending lazy synchronization |
| `dirty_xmm_mask` | `uint32_t` | 4-byte | Bitmask of dirty XMM registers pending lazy synchronization |

---

## 2. ABI Transition & Exception Protocols

### 16-Byte Stack Alignment Verification
- Prior to executing guest procedure calls, `ctx.VerifyStackAlignment()` evaluates `(rsp & 0xF) == 0`.
- Misaligned stack pointers (`rsp & 0xF != 0`) are automatically aligned down to 16-byte boundaries (`ctx.rsp = ctx.rsp & ~0x0Fu`) to preserve SysV AMD64 / AAPCS64 calling convention invariants.

### Lazy Register Synchronization Protocol
- Host register modifications set corresponding bits in `dirty_gpr_mask` or `dirty_xmm_mask`.
- `ctx.FlushLazyRegisters()` clears masks when registers are committed back to `GuestCpuContext` memory.

### Exception-Safe Transition Frames
- `ExecuteBlock` encapsulates JIT block execution inside exception-safe boundary frames, trapping memory faults or illegal instruction traps and preserving process stability.
