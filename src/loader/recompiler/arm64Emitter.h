// arm64Emitter.h
//
// ARM64 Backend Code Emitter & Instruction Encoder for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_ARM64_EMITTER_H
#define LOADER_RECOMPILER_ARM64_EMITTER_H

#include "common/common.h"
#include "loader/recompiler/x86RecompilerIR.h"

#include <cstdint>
#include <vector>

namespace Loader::Recompiler {

enum class Arm64Reg : uint8_t {
	X0 = 0, X1, X2, X3, X4, X5, X6, X7,
	X8, X9, X10, X11, X12, X13, X14, X15,
	X16, X17, X18, X19, X20, X21, X22, X23,
	X24, X25, X26, X27, X28, X29, X30, SP = 31, XZR = 31
};

struct Arm64Relocation {
	size_t   host_code_offset = 0;
	uint64_t target_guest_rip = 0;
	bool     is_branch_link   = false;
};

class Arm64Emitter {
public:
	Arm64Emitter() = default;
	~Arm64Emitter() = default;

	KYTY_CLASS_NO_COPY(Arm64Emitter);

	void Clear() noexcept {
		m_code.clear();
		m_relocations.clear();
	}

	[[nodiscard]] const std::vector<uint32_t>& GetCode() const noexcept { return m_code; }
	[[nodiscard]] size_t GetCodeSizeBytes() const noexcept { return m_code.size() * sizeof(uint32_t); }
	[[nodiscard]] const std::vector<Arm64Relocation>& GetRelocations() const noexcept { return m_relocations; }

	void AddRelocation(size_t host_offset, uint64_t target_rip, bool is_link = false) {
		m_relocations.push_back({host_offset, target_rip, is_link});
	}

	void SetInstruction(size_t index, uint32_t inst) noexcept {
		if (index < m_code.size()) {
			m_code[index] = inst;
		}
	}

	// GPR Mapping: x86-64 GPR -> ARM64 Host GPR
	static Arm64Reg MapX86ToArm64Reg(X86Reg reg) noexcept;

	// Low-level Instruction Emission
	void Emit32(uint32_t inst);
	void EmitNop();

	// 1. Integer Instructions
	void EmitAndReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitOrrReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitEorReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitMovReg(Arm64Reg dst, Arm64Reg src, bool sf = true);
	void EmitMovz(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw = 0, bool sf = true);
	void EmitMovk(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw = 0, bool sf = true);
	void EmitMovn(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw = 0, bool sf = true);
	void EmitMovImm64(Arm64Reg dst, uint64_t imm);
	void EmitAddReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitAddImm(Arm64Reg dst, Arm64Reg src, uint32_t imm, bool sf = true);
	void EmitSubReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitSubImm(Arm64Reg dst, Arm64Reg src, uint32_t imm, bool sf = true);
	void EmitAdc(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitSbc(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitCmpReg(Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitCmpImm(Arm64Reg src, uint32_t imm, bool sf = true);
	void EmitTstReg(Arm64Reg src1, Arm64Reg src2, bool sf = true);

	// 2. Memory Instructions
	void EmitLdr64(Arm64Reg dst, Arm64Reg base, int32_t offset);
	void EmitStr64(Arm64Reg src, Arm64Reg base, int32_t offset);
	void EmitLdur64(Arm64Reg dst, Arm64Reg base, int32_t simm9);
	void EmitStur64(Arm64Reg src, Arm64Reg base, int32_t simm9);
	void EmitLdp64(Arm64Reg dst1, Arm64Reg dst2, Arm64Reg base, int32_t offset);
	void EmitStp64(Arm64Reg src1, Arm64Reg src2, Arm64Reg base, int32_t offset);
	void EmitLdp64PostIndex(Arm64Reg dst1, Arm64Reg dst2, Arm64Reg base, int32_t offset);
	void EmitStp64PreIndex(Arm64Reg src1, Arm64Reg src2, Arm64Reg base, int32_t offset);

	// 3. Branch Instructions
	void EmitB(int32_t offset_words);
	void EmitBl(int32_t offset_words);
	void EmitBr(Arm64Reg reg);
	void EmitBlr(Arm64Reg reg);
	void EmitRet();
	void EmitCbz(Arm64Reg reg, int32_t offset_words, bool sf = true);
	void EmitCbnz(Arm64Reg reg, int32_t offset_words, bool sf = true);
	void EmitTbz(Arm64Reg reg, uint8_t bit_num, int32_t offset_words);
	void EmitTbnz(Arm64Reg reg, uint8_t bit_num, int32_t offset_words);
	void EmitBcc(X86Condition cond, bool invert, int32_t offset_words);

	// 4. Shift Instructions
	void EmitLsl(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf = true);
	void EmitLsr(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf = true);
	void EmitAsr(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf = true);
	void EmitRor(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf = true);

	// 5. Multiply Instructions
	void EmitMulReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitMadd(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, Arm64Reg acc, bool sf = true);
	void EmitMsub(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, Arm64Reg acc, bool sf = true);
	void EmitUmulh(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2);
	void EmitSmulh(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2);

	// 6. Bit Manipulation & Count Instructions
	void EmitClz(Arm64Reg dst, Arm64Reg src, bool sf = true);
	void EmitRbit(Arm64Reg dst, Arm64Reg src, bool sf = true);
	void EmitBicReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf = true);
	void EmitUbfx(Arm64Reg dst, Arm64Reg src, uint8_t lsb, uint8_t width, bool sf = true);

	// 7. Memory Barriers & Synchronization
	void EmitDmb(uint8_t option = 0x0B);
	void EmitDsb(uint8_t option = 0x0B);
	void EmitIsb();
	void EmitDmbIsh()   { EmitDmb(0x0B); }
	void EmitDmbIshld() { EmitDmb(0x09); }
	void EmitDmbIshst() { EmitDmb(0x0A); }

	// Translation Engine
	bool CompileBlock(const RecompilerBasicBlock& block);

private:
	std::vector<uint32_t>       m_code;
	std::vector<Arm64Relocation> m_relocations;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_EMITTER_H
