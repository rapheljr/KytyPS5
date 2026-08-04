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
	X24, X25, X26, X27, X28, X29, X30, SP = 31
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

	// GPR Mapping: x86-64 GPR -> ARM64 Host GPR
	static Arm64Reg MapX86ToArm64Reg(X86Reg reg) noexcept;

	// Instruction Emission Methods
	void Emit32(uint32_t inst);
	void EmitNop();
	void EmitRet();
	void EmitMovReg(Arm64Reg dst, Arm64Reg src);
	void EmitMovImm64(Arm64Reg dst, uint64_t imm);
	void EmitAddReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2);
	void EmitAddImm(Arm64Reg dst, Arm64Reg src, uint32_t imm);
	void EmitSubReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2);
	void EmitSubImm(Arm64Reg dst, Arm64Reg src, uint32_t imm);
	void EmitMulReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2);
	void EmitCmpReg(Arm64Reg src1, Arm64Reg src2);
	void EmitCmpImm(Arm64Reg src, uint32_t imm);
	void EmitB(int32_t offset_words);
	void EmitBcc(X86Condition cond, bool invert, int32_t offset_words);
	void EmitBlr(Arm64Reg reg);
	void EmitLdr64(Arm64Reg dst, Arm64Reg base, int32_t offset);
	void EmitStr64(Arm64Reg src, Arm64Reg base, int32_t offset);
	void EmitStp64(Arm64Reg src1, Arm64Reg src2, Arm64Reg base, int32_t offset);
	void EmitLdp64(Arm64Reg dst1, Arm64Reg dst2, Arm64Reg base, int32_t offset);

	// Translation Engine
	bool CompileBlock(const RecompilerBasicBlock& block);

private:
	std::vector<uint32_t>       m_code;
	std::vector<Arm64Relocation> m_relocations;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_EMITTER_H
