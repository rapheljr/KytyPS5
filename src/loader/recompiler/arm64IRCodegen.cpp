// arm64IRCodegen.cpp
//
// Target-Independent Compiler IR to Native ARM64 Code Generator.

#include "loader/recompiler/arm64IRCodegen.h"
#include "loader/recompiler/x86RuntimeBridge.h"
#include "common/profiler.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

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

Arm64FpReg Arm64IRCodegen::MapOperandToArm64FpReg(const VirtualReg& vreg) const noexcept {
	if (vreg.phys_pin >= 0 && vreg.phys_pin < 16) {
		return static_cast<Arm64FpReg>(vreg.phys_pin);
	}
	return static_cast<Arm64FpReg>(vreg.id % 16);
}

bool Arm64IRCodegen::CompileCFG(ControlFlowGraph& cfg, Arm64Emitter& emitter) {
	KYTY_PROFILER_FUNCTION();

	// 1. Execute Linear Scan Register Allocator over Compiler IR CFG
	auto alloc_res = m_allocator.Allocate(cfg);
	if (!alloc_res.success) return false;

	Arm64FpSimdEmitter fp_emitter(emitter);

	// 2. AAPCS64 Prologue Generation
	// Frame layout (256 bytes, 16-byte aligned):
	// [SP, 0]:   FP (X29), LR (X30)
	// [SP, 16]:  X19, X20
	// [SP, 32]:  X21, X22
	// [SP, 48]:  X23, X24
	// [SP, 64]:  X25, X26
	// [SP, 80]:  X27, X28
	// [SP, 96..127]: Alignment pad / context slots
	// [SP, 128..255]: Callee-saved SIMD Q8..Q15 (16 bytes each)
	constexpr uint32_t stack_frame_size = 256;

	// Push FP (X29) & LR (X30) with pre-indexing
	emitter.EmitStp64PreIndex(Arm64Reg::X29, Arm64Reg::X30, Arm64Reg::SP, -static_cast<int32_t>(stack_frame_size));

	// Preserve callee-saved host GPRs
	emitter.EmitStp64(Arm64Reg::X19, Arm64Reg::X20, Arm64Reg::SP, 16);
	emitter.EmitStp64(Arm64Reg::X21, Arm64Reg::X22, Arm64Reg::SP, 32);
	emitter.EmitStp64(Arm64Reg::X23, Arm64Reg::X24, Arm64Reg::SP, 48);
	emitter.EmitStp64(Arm64Reg::X25, Arm64Reg::X26, Arm64Reg::SP, 64);
	emitter.EmitStp64(Arm64Reg::X27, Arm64Reg::X28, Arm64Reg::SP, 80);

	// Preserve callee-saved host SIMD registers (Q8..Q15)
	for (int i = 8; i < 16; ++i) {
		fp_emitter.EmitStrQ(static_cast<Arm64FpReg>(i), Arm64Reg::SP, 128 + (i - 8) * 16);
	}

	// Pin X19 as GuestCpuContext pointer (passed in X0 by caller)
	emitter.EmitMovReg(Arm64Reg::X19, Arm64Reg::X0);

	// Load all guest GPRs (RAX..R15) from [X19 + offset] into host registers X0..X15
	emitter.EmitLdp64(Arm64Reg::X0, Arm64Reg::X1, Arm64Reg::X19, 0);    // RAX, RCX
	emitter.EmitLdp64(Arm64Reg::X2, Arm64Reg::X3, Arm64Reg::X19, 16);   // RDX, RBX
	emitter.EmitLdp64(Arm64Reg::X4, Arm64Reg::X5, Arm64Reg::X19, 32);   // RSP, RBP
	emitter.EmitLdp64(Arm64Reg::X6, Arm64Reg::X7, Arm64Reg::X19, 48);   // RSI, RDI
	emitter.EmitLdp64(Arm64Reg::X8, Arm64Reg::X9, Arm64Reg::X19, 64);   // R8,  R9
	emitter.EmitLdp64(Arm64Reg::X10, Arm64Reg::X11, Arm64Reg::X19, 80); // R10, R11
	emitter.EmitLdp64(Arm64Reg::X12, Arm64Reg::X13, Arm64Reg::X19, 96); // R12, R13
	emitter.EmitLdp64(Arm64Reg::X14, Arm64Reg::X15, Arm64Reg::X19, 112);// R14, R15

	// Load all guest Vector SIMD registers (XMM0..XMM15) from [X19 + 128 + i*16] into Q0..Q15
	for (int i = 0; i < 16; ++i) {
		fp_emitter.EmitLdrQ(static_cast<Arm64FpReg>(i), Arm64Reg::X19, 128 + i * 16);
	}

	std::vector<BasicBlock*> rpo = cfg.ComputeReversePostOrder();
	if (rpo.empty()) return false;

	auto emit_epilogue = [&]() {
		// Store updated guest GPRs back into [X19 + offset]
		emitter.EmitStp64(Arm64Reg::X0, Arm64Reg::X1, Arm64Reg::X19, 0);
		emitter.EmitStp64(Arm64Reg::X2, Arm64Reg::X3, Arm64Reg::X19, 16);
		emitter.EmitStp64(Arm64Reg::X4, Arm64Reg::X5, Arm64Reg::X19, 32);
		emitter.EmitStp64(Arm64Reg::X6, Arm64Reg::X7, Arm64Reg::X19, 48);
		emitter.EmitStp64(Arm64Reg::X8, Arm64Reg::X9, Arm64Reg::X19, 64);
		emitter.EmitStp64(Arm64Reg::X10, Arm64Reg::X11, Arm64Reg::X19, 80);
		emitter.EmitStp64(Arm64Reg::X12, Arm64Reg::X13, Arm64Reg::X19, 96);
		emitter.EmitStp64(Arm64Reg::X14, Arm64Reg::X15, Arm64Reg::X19, 112);

		// Store updated guest Vector SIMD registers back into [X19 + 128 + i*16]
		for (int i = 0; i < 16; ++i) {
			fp_emitter.EmitStrQ(static_cast<Arm64FpReg>(i), Arm64Reg::X19, 128 + i * 16);
		}

		// Restore callee-saved host SIMD registers (Q8..Q15)
		for (int i = 8; i < 16; ++i) {
			fp_emitter.EmitLdrQ(static_cast<Arm64FpReg>(i), Arm64Reg::SP, 128 + (i - 8) * 16);
		}

		// Restore callee-saved host GPRs
		emitter.EmitLdp64(Arm64Reg::X27, Arm64Reg::X28, Arm64Reg::SP, 80);
		emitter.EmitLdp64(Arm64Reg::X25, Arm64Reg::X26, Arm64Reg::SP, 64);
		emitter.EmitLdp64(Arm64Reg::X23, Arm64Reg::X24, Arm64Reg::SP, 48);
		emitter.EmitLdp64(Arm64Reg::X21, Arm64Reg::X22, Arm64Reg::SP, 32);
		emitter.EmitLdp64(Arm64Reg::X19, Arm64Reg::X20, Arm64Reg::SP, 16);

		// Restore FP, LR and pop stack frame
		emitter.EmitLdp64PostIndex(Arm64Reg::X29, Arm64Reg::X30, Arm64Reg::SP, static_cast<int32_t>(stack_frame_size));
		emitter.EmitRet();
	};

	struct BranchFixup {
		size_t   inst_idx       = 0;
		uint64_t target_rip     = 0;
		bool     is_conditional = false;
		uint32_t cond_code      = 0;
	};
	std::vector<BranchFixup> fixups;
	std::unordered_map<uint64_t, size_t> rip_to_inst_idx;

	// 3. Emit Body Instructions
	for (BasicBlock* block : rpo) {
		for (const auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			if (inst->GetGuestRip() != 0 && rip_to_inst_idx.find(inst->GetGuestRip()) == rip_to_inst_idx.end()) {
				rip_to_inst_idx[inst->GetGuestRip()] = emitter.GetCode().size();
			}

			Arm64Reg   dst_reg = inst->HasDst() ? MapOperandToArm64Reg(inst->GetDst()) : Arm64Reg::X0;
			Arm64FpReg dst_fp  = inst->HasDst() ? MapOperandToArm64FpReg(inst->GetDst()) : Arm64FpReg::V0;
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
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitSubReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Mul:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitMulReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitMulReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::And:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitAndReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitAndReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Or:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitOrrReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitOrrReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Xor:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitEorReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitEorReg(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Shl:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitLsl(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitLsl(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::LShr:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitLsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitLsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::AShr:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitAsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitAsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Ror:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitRor(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
							emitter.EmitRor(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
						}
					}
					break;

				case IROpcode::Clz:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						emitter.EmitClz(dst_reg, MapOperandToArm64Reg(ops[0].vreg));
					}
					break;

				case IROpcode::Ctz:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						emitter.EmitRbit(dst_reg, MapOperandToArm64Reg(ops[0].vreg));
						emitter.EmitClz(dst_reg, dst_reg);
					}
					break;

				case IROpcode::Popcnt:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						Arm64Reg src_r = MapOperandToArm64Reg(ops[0].vreg);
						emitter.Emit32(0x9E670000u | ((static_cast<uint32_t>(src_r) & 0x1Fu) << 5) | 16u);
						fp_emitter.EmitCnt8B(Arm64FpReg::V16, Arm64FpReg::V16);
						fp_emitter.EmitUaddlv8B(Arm64FpReg::V16, Arm64FpReg::V16);
						emitter.Emit32(0x9E660000u | (16u << 5) | (static_cast<uint32_t>(dst_reg) & 0x1Fu));
					}
					break;

				case IROpcode::Blsi:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						Arm64Reg src_r = MapOperandToArm64Reg(ops[0].vreg);
						emitter.EmitSubReg(Arm64Reg::X16, Arm64Reg::XZR, src_r);
						emitter.EmitAndReg(dst_reg, src_r, Arm64Reg::X16);
					}
					break;

				case IROpcode::Blsr:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						Arm64Reg src_r = MapOperandToArm64Reg(ops[0].vreg);
						emitter.EmitSubImm(Arm64Reg::X16, src_r, 1);
						emitter.EmitAndReg(dst_reg, src_r, Arm64Reg::X16);
					}
					break;

				case IROpcode::Blsmsk:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						Arm64Reg src_r = MapOperandToArm64Reg(ops[0].vreg);
						emitter.EmitSubImm(Arm64Reg::X16, src_r, 1);
						emitter.EmitEorReg(dst_reg, src_r, Arm64Reg::X16);
					}
					break;

				case IROpcode::Shlx:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitLsl(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::Shrx:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitLsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::Sarx:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitAsr(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::Rorx:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitRor(dst_reg, MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					} else if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsImmInt()) {
						emitter.EmitMovImm64(Arm64Reg::X16, static_cast<uint64_t>(ops[1].imm_int));
						emitter.EmitRor(dst_reg, MapOperandToArm64Reg(ops[0].vreg), Arm64Reg::X16);
					}
					break;

				case IROpcode::Bextr:
					if (ops.size() >= 3 && ops[0].IsVReg() && ops[1].IsImmInt() && ops[2].IsImmInt()) {
						emitter.EmitUbfx(dst_reg, MapOperandToArm64Reg(ops[0].vreg), static_cast<uint8_t>(ops[1].imm_int), static_cast<uint8_t>(ops[2].imm_int));
					}
					break;

				case IROpcode::VectorInsert:
					if (ops.size() >= 3 && ops[1].IsVReg() && ops[2].IsImmInt()) {
						uint8_t lane = static_cast<uint8_t>(ops[2].imm_int & 0x03);
						fp_emitter.EmitIns4S(dst_fp, lane, MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::VectorExtract:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsImmInt()) {
						uint8_t lane = static_cast<uint8_t>(ops[1].imm_int & 0x03);
						fp_emitter.EmitUmov4S(dst_reg, MapOperandToArm64FpReg(ops[0].vreg), lane);
					}
					break;

				case IROpcode::VectorBlend:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVbsl16B(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VectorZeroExtend:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						fp_emitter.EmitZip14S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[0].vreg));
					}
					break;

				case IROpcode::Crc32:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						Arm64Reg rn = MapOperandToArm64Reg(ops[0].vreg);
						Arm64Reg rm = MapOperandToArm64Reg(ops[1].vreg);
						uint32_t inst_crc = 0x9AC04800u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
						                    ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(dst_reg) & 0x1Fu);
						emitter.Emit32(inst_crc);
					}
					break;

				case IROpcode::Cmp:
					if (ops.size() >= 2) {
						if (ops[0].IsVReg() && ops[1].IsVReg()) {
							emitter.EmitCmpReg(MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
						} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
							emitter.EmitCmpImm(MapOperandToArm64Reg(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
						}
					}
					break;

				case IROpcode::Test:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						emitter.EmitTstReg(MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].vreg));
					}
					break;

				case IROpcode::Load:
					if (ops.size() >= 1 && ops[0].IsMemoryRef()) {
						emitter.EmitLdr64(dst_reg, MapOperandToArm64Reg(ops[0].mem_ref.base), ops[0].mem_ref.disp);
					}
					break;

				case IROpcode::Store:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsMemoryRef()) {
						emitter.EmitStr64(MapOperandToArm64Reg(ops[0].vreg), MapOperandToArm64Reg(ops[1].mem_ref.base), ops[1].mem_ref.disp);
						emitter.EmitDmbIshst();
					}
					break;

				// ─── Vector SIMD Lowering (128-Bit NEON) ─────────────────────
				case IROpcode::VecMov:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						fp_emitter.EmitVorr16B(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[0].vreg));
					}
					break;

				case IROpcode::VecLoad:
					if (ops.size() >= 1 && ops[0].IsMemoryRef()) {
						fp_emitter.EmitLdrQ(dst_fp, MapOperandToArm64Reg(ops[0].mem_ref.base), ops[0].mem_ref.disp);
					}
					break;

				case IROpcode::VecStore:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsMemoryRef()) {
						fp_emitter.EmitStrQ(MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64Reg(ops[1].mem_ref.base), ops[1].mem_ref.disp);
					}
					break;

				case IROpcode::VecAdd:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVadd4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecSub:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVsub4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecMul:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVmul4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecDiv:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVdiv4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecSqrt:
					if (ops.size() >= 1 && ops[0].IsVReg()) {
						fp_emitter.EmitFsqrt4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg));
					}
					break;

				case IROpcode::VecAnd:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVand16B(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecOr:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVorr16B(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecXor:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitVeor16B(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecMin:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitFmin4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::VecMax:
					if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
						fp_emitter.EmitFmax4S(dst_fp, MapOperandToArm64FpReg(ops[0].vreg), MapOperandToArm64FpReg(ops[1].vreg));
					}
					break;

				case IROpcode::BranchCond: {
					uint32_t cond_code = 0x0;
					switch (inst->GetCondition()) {
						case IRCondition::Equal:          cond_code = 0x0; break;
						case IRCondition::NotEqual:       cond_code = 0x1; break;
						case IRCondition::AboveOrEqual:   cond_code = 0x2; break;
						case IRCondition::Below:          cond_code = 0x3; break;
						case IRCondition::Overflow:       cond_code = 0x6; break;
						case IRCondition::NoOverflow:     cond_code = 0x7; break;
						case IRCondition::Above:          cond_code = 0x8; break;
						case IRCondition::BelowOrEqual:   cond_code = 0x9; break;
						case IRCondition::GreaterOrEqual: cond_code = 0xA; break;
						case IRCondition::Less:           cond_code = 0xB; break;
						case IRCondition::Greater:        cond_code = 0xC; break;
						case IRCondition::LessOrEqual:    cond_code = 0xD; break;
					}
					uint64_t target_rip = 0;
					if (!ops.empty() && ops[0].IsImmInt()) {
						target_rip = static_cast<uint64_t>(ops[0].imm_int);
					}
					size_t branch_idx = emitter.GetCode().size();
					emitter.Emit32(0x54000000u | (cond_code & 0x0Fu)); // Placeholder
					fixups.push_back({branch_idx, target_rip, true, cond_code});
					break;
				}

				case IROpcode::Jump: {
					if (!ops.empty() && ops[0].IsImmInt()) {
						uint64_t target_rip = static_cast<uint64_t>(ops[0].imm_int);
						size_t branch_idx = emitter.GetCode().size();
						emitter.EmitB(0); // Placeholder
						fixups.push_back({branch_idx, target_rip, false, 0});
					} else if (!ops.empty() && ops[0].IsVReg()) {
						emitter.EmitBr(MapOperandToArm64Reg(ops[0].vreg));
					}
					break;
				}

				case IROpcode::Call: {
					if (!ops.empty() && ops[0].IsImmInt()) {
						uint64_t target_rip = static_cast<uint64_t>(ops[0].imm_int);
						emitter.AddRelocation(emitter.GetCodeSizeBytes(), target_rip, true);
						emitter.EmitBl(0);
					} else if (!ops.empty() && ops[0].IsVReg()) {
						emitter.EmitBlr(MapOperandToArm64Reg(ops[0].vreg));
					}
					break;
				}

				case IROpcode::Syscall: {
					// Store next RIP to guest context [X19 + offsetof(GuestCpuContext, rip)]
					uint64_t next_rip = inst->GetGuestRip() + 2;
					emitter.EmitMovImm64(Arm64Reg::X16, next_rip);
					emitter.EmitStr64(Arm64Reg::X16, Arm64Reg::X19, static_cast<int32_t>(offsetof(GuestCpuContext, rip)));
					emit_epilogue();
					break;
				}

				case IROpcode::Return:
					emit_epilogue();
					break;

				default:
					emitter.EmitNop();
					break;
			}
		}
	}

	// 4. Resolve PC-Relative Branch Displacements
	for (const auto& fixup : fixups) {
		auto it = (fixup.target_rip != 0) ? rip_to_inst_idx.find(fixup.target_rip) : rip_to_inst_idx.end();
		if (it != rip_to_inst_idx.end()) {
			// Intra-CFG target resolution
			size_t target_inst_idx = it->second;
			int32_t offset_words = static_cast<int32_t>(target_inst_idx) - static_cast<int32_t>(fixup.inst_idx);
			if (fixup.is_conditional) {
				uint32_t imm19 = static_cast<uint32_t>(offset_words) & 0x7FFFFu;
				emitter.SetInstruction(fixup.inst_idx, 0x54000000u | (imm19 << 5u) | (fixup.cond_code & 0x0Fu));
			} else {
				uint32_t imm26 = static_cast<uint32_t>(offset_words) & 0x03FFFFFFu;
				emitter.SetInstruction(fixup.inst_idx, 0x14000000u | imm26);
			}
		} else {
			// External / out-of-CFG jump target -> Register relocation for block linker
			emitter.AddRelocation(fixup.inst_idx * sizeof(uint32_t), fixup.target_rip, false);
		}
	}

	// Always ensure AAPCS64 Epilogue at end of block
	emit_epilogue();
	return true;
}

} // namespace Loader::Recompiler
