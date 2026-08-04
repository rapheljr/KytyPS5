# KytyPS5 Target-Independent Compiler IR Node & Architecture Specification

## Overview
This specification details the design, SSA invariants, control flow graph (CFG) representations, IR instruction opcodes, optimization pass semantics, and codegen interfaces for the target-independent **Compiler Intermediate Representation (IR)** in **KytyPS5**.

---

## 1. Virtual Registers & Data Types

Every SSA value in the IR is represented as a virtual register (`VirtualReg`) or a constant immediate `Value`.

### Data Types (`DataType`)
- `Int8`: 8-bit integer
- `Int16`: 16-bit integer
- `Int32`: 32-bit integer
- `Int64`: 64-bit integer
- `Float32`: 32-bit IEEE 754 floating-point
- `Float64`: 64-bit IEEE 754 floating-point
- `Vec128`: 128-bit SIMD vector

### Virtual Register Structure
- `id`: Globally unique 32-bit integer identifier (`v1, v2, v3...`).
- `type`: Target-independent `DataType`.
- `phys_pin`: Optional physical register pin (-1 if unpinned, >= 0 for architectural hardware register mapping).

---

## 2. IR Opcodes (`IROpcode`) & Node Specification

| Opcode Category | IROpcode Token | Operands | Description & SSA Semantics |
| :--- | :--- | :--- | :--- |
| **Special** | `Nop` | Optional VReg/Imm | No-operation marker; used during optimization pass folding and dead code elimination. |
| **Arithmetic** | `Add` | `src1`, `src2` | Binary addition: `dst = src1 + src2`. |
| | `Sub` | `src1`, `src2` | Binary subtraction: `dst = src1 - src2`. |
| | `Mul` | `src1`, `src2` | Binary multiplication: `dst = src1 * src2`. |
| | `SDiv` | `src1`, `src2` | Signed integer division: `dst = src1 / src2`. |
| | `UDiv` | `src1`, `src2` | Unsigned integer division: `dst = src1 / src2`. |
| **Bitwise** | `And` | `src1`, `src2` | Bitwise AND: `dst = src1 & src2`. |
| | `Or` | `src1`, `src2` | Bitwise OR: `dst = src1 \| src2`. |
| | `Xor` | `src1`, `src2` | Bitwise XOR: `dst = src1 ^ src2`. |
| | `Shl` | `src1`, `shift` | Logical shift left: `dst = src1 << shift`. |
| | `LShr` | `src1`, `shift` | Logical shift right: `dst = src1 >> shift`. |
| | `AShr` | `src1`, `shift` | Arithmetic shift right: `dst = src1 >> shift` (sign extended). |
| | `Rol` | `src1`, `count` | Rotate left: `dst = rotl(src1, count)`. |
| | `Ror` | `src1`, `count` | Rotate right: `dst = rotr(src1, count)`. |
| | `Neg` | `src` | Two's complement negation: `dst = -src`. |
| | `Not` | `src` | Bitwise NOT: `dst = ~src`. |
| **Extensions** | `ZExt` | `src` | Zero extension to target bitwidth. |
| | `SExt` | `src` | Sign extension to target bitwidth. |
| | `Trunc` | `src` | Bitwidth truncation. |
| **Memory** | `Load` | `mem_ref` | Load value from memory address `[base + disp]`. |
| | `Store` | `val`, `mem_ref` | Store value to memory address `[base + disp]`. |
| **Control Flow** | `Jump` | `target_block` | Unconditional jump to target `BasicBlock`. |
| | `BranchCond` | `cond`, `true_bb`, `false_bb` | Conditional branch based on boolean condition input. |
| | `Return` | None | Return execution to caller. |
| | `Switch` | `val`, `cases...` | Multi-way control flow branch. |
| | `Unreachable` | None | Control flow trap marker. |
| **SSA Nodes** | `Phi` | `(pred_bb, val)...` | SSA Static Single Assignment Phi node selecting input value based on predecessor basic block. |
| | `Select` | `cond`, `val_true`, `val_false` | Conditional move/select: `dst = cond ? val_true : val_false`. |
| | `SetCond` | `cond`, `src1`, `src2` | Set destination register to 1/0 based on comparison condition evaluation. |

---

## 3. Dominator Tree & SSA Optimization Passes

The IR optimization pipeline (`PassManager`) executes 9 target-independent passes over the CFG using dominator analysis:

1. **Constant Propagation (`ConstantPropagationPass`)**: Propagates constant VirtualReg definitions across all dominated use sites.
2. **Constant Folding (`ConstantFoldingPass`)**: Evaluates constant expressions at compile time.
3. **Copy Propagation (`CopyPropagationPass`)**: Eliminates redundant register copies (`v2 = v1`).
4. **Algebraic Simplification (`AlgebraicSimplificationPass`)**: Applies mathematical identity rules (`x + 0 -> x`, `x * 1 -> x`, `x ^ x -> 0`, `x & 0 -> 0`, `x * 0 -> 0`, `x - x -> 0`).
5. **Common Subexpression Elimination (`CSE`)**: Reuses identical expressions evaluated earlier within dominated blocks.
6. **Branch Simplification (`BranchSimplificationPass`)**: Folds constant conditional branches and eliminates unreachable CFG blocks/edges.
7. **Dead Code Elimination (`DCE`)**: Prunes unused instructions having zero side effects and empty use chains.
8. **Dead Store Elimination (`DSE`)**: Removes redundant memory stores overwritten before subsequent loads.
9. **Register Coalescing (`RegisterCoalescingPass`)**: Merges overlapping virtual register live ranges across copy boundaries.

---

## 4. Diagnostics & Verification Infrastructure

- **`IRVerifier`**: Validates CFG predecessor/successor symmetry, single-definition property, and operand dominance invariants.
- **`IRPrinter`**: Generates formatted, human-readable disassembly string representations of basic blocks, SSA registers, and CFG structures.
- **`GraphvizExporter`**: Exports CFG and Dominator Tree node relationships to `.dot` syntax for visual rendering with Graphviz.
