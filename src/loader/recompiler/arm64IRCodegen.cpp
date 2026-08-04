// arm64IRCodegen.cpp
//
// Target-Independent Compiler IR to Native ARM64 Code Generator.

#include "loader/recompiler/arm64IRCodegen.h"

namespace Loader::Recompiler {

Arm64Reg Arm64IRCodegen::MapOperandToArm64Reg(const VirtualReg& vreg) const noexcept {
	const auto* interval = m_allocator.GetIntervalForVReg(vreg.id);
	if (interval && interval->assigned_phys_reg >= 0) {
		return static_cast<Arm64Reg>(interval->assigned_phys_reg);
	}
	if (vreg.phys_pin >= 0 && vreg.phys_pin < 16) {
		return Arm64Emitter::MapX86ToArm64Reg(static_cast<X86Reg>(vreg.phys_pin));
	}
	return static_cast<Arm64Reg>(vreg.id % 16);
}

bool Arm64IRCodegen::CompileCFG(ControlFlowGraph& cfg, Arm64Emitter& emitter) {
	// 1. Execute Linear Scan Register Allocator over Compiler IR CFG
	auto alloc_res = m_allocator.Allocate(cfg);
	if (!alloc_res.success) return false;

	// 2. AAPCS64 Prologue Generation
	uint32_t stack_frame_size = 16 + alloc_res.total_spill_bytes;
	stack_frame_size = (stack_frame_size + 15) & ~15; // 16-byte align

	// Push FP (X29) & LR (X30)
	emitter.EmitStp64(Arm64Reg::X29, Arm64Reg::X30, Arm64Reg::SP, -static_cast<int32_t>(stack_frame_size));

	std::vector<BasicBlock*> rpo = cfg.ComputeReversePostOrder();
	if (rpo.empty()) return false;

	// 3. Emit Body Instructions
	for (BasicBlock* block : rpo) {
		for (const auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			Arm64Reg dst_reg = inst->HasDst() ? MapOperandToArm64Reg(inst->GetDst()) : Arm64Reg::X0;
			const auto& ops = inst->GetOperands();

			switch (inst->GetOpcode()) {
				case IROpcode::Nop:
					emitter.EmitNop();
					break;

				case IROpcode::Add:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitAddReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitAddImm(dst_reg, MapOperandToArm64Reg(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
						} else if (ops[0].IsImmInt()) {
							emitter.EmitMovImm64(dst_reg, static_cast<uint64_t>(ops[0].imm_int));
						}
					} else if (ops.size() == 1) {
						if (ops[0].IsVReg()) emitter.EmitMovReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg));
						else if (ops[0].IsImmInt()) emitter.EmitMovImm64(dst_reg, static_cast<uint64_t>(ops[0].imm_int));
					}
					break;

				case IROpcode::Sub:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitSubReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitSubImm(dst_reg, MapOperandToArm64Reg(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
						}
					}
					break;

				case IROpcode::Mul:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitMulReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::Load:
					if (ops.size() >= 1 && ops[0].IsMemoryRef()) {
						emitter.EmitLdr64(dst_reg, Arm64Reg::SP, ops[0].mem_ref.disp);
					}
					break;

				case IROpcode::Store:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsMemoryRef()) {
						emitter.EmitStr64(MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::SP, ops[1].mem_ref.disp);
					}
					break;

				case IROpcode::Return:
					// AAPCS64 Epilogue Generation
					emitter.EmitLdp64(Arm64Reg::X29, Arm64Reg::X30, Arm64Reg::SP, static_cast<int32_t>(stack_frame_size));
					emitter.EmitRet();
					break;

				default:
					emitter.EmitNop();
					break;
			}
		}
	}

	// Always ensure AAPCS64 Epilogue (LDP X29, X30, [SP], #32 + RET) at end of function
	emitter.EmitLdp64(Arm64Reg::X29, Arm64Reg::X30, Arm64Reg::SP, static_cast<int32_t>(stack_frame_size));
	emitter.EmitRet();
	return true;
}

} // namespace Loader::Recompiler
