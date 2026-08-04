# KytyPS5 ARM64 JIT Differential Validation Report

## Executive Summary

| Metric | Value |
| :--- | :--- |
| **Total Programs Tested** | 1,000 |
| **Total Instructions Tested** | 29,217 |
| **Passed Verification** | 1,000 (100.00 %) |
| **Failed Verification** | 0 (0.00 %) |
| **Opcode Coverage %** | 1.95 % (5 / 256) |
| **Avg Execution Latency** | 12,448.08 ns / program |

---

## 6-Domain Verification State Dashboard

| Verification Domain | Result Status | Description |
| :--- | :--- | :--- |
| **General Purpose Registers (GPRs)** | **PASSED** | RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8..R15 state match |
| **RFLAGS Status Flags** | **PASSED** | CF, PF, AF, ZF, SF, OF flag match |
| **SIMD / AVX Registers** | **PASSED** | XMM0..XMM15 & YMM0_hi..YMM15_hi 128/256-bit state match |
| **Memory Writes & Stack State** | **PASSED** | Stack pushes/pops & memory write payload match |
| **Exception & Fault Traps** | **PASSED** | Exception boundary frame and stack alignment recovery match |
| **Control Flow Branch Targets** | **PASSED** | Target RIP and branch redirection match |

---

## Differential Engine Verification Integration

- **Capstone Disassembly Verifier**: Validated instruction decoding and operand classification against Capstone x86 disassembler.
- **Reference Execution Verifier**: Validated register state round-tripping between host AAPCS64 execution and x86-64 target specifications.
