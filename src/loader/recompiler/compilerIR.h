// compilerIR.h
//
// Target-Independent Compiler Intermediate Representation (IR) & Control Flow Graph (CFG).

#ifndef LOADER_RECOMPILER_COMPILER_IR_H
#define LOADER_RECOMPILER_COMPILER_IR_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Loader::Recompiler {

enum class DataType : uint8_t {
	None = 0,
	Int8,
	Int16,
	Int32,
	Int64,
	Float32,
	Float64,
	Vec128
};

enum class IROpcode : uint16_t {
	Nop = 0,
	// Arithmetic & Logical
	Add,
	Sub,
	Mul,
	SDiv,
	UDiv,
	And,
	Or,
	Xor,
	Shl,
	LShr,
	AShr,
	Rol,
	Ror,
	Neg,
	Not,
	// Bit Manipulation & Count
	Clz,
	Ctz,
	Popcnt,
	Andn,
	Bextr,
	Blsi,
	Blsr,
	Blsmsk,
	Rorx,
	Sarx,
	Shlx,
	Shrx,
	Crc32,
	// Casts & Extensions
	ZExt,
	SExt,
	Trunc,
	// Memory Operations
	Load,
	Store,
	// Control Flow
	Jump,
	BranchCond,
	Call,
	Syscall,
	Return,
	Switch,
	Unreachable,
	// SSA Special & Conditionals
	Phi,
	Select,
	SetCond,
	Cmp,
	Test,
	// Floating-Point Scalar
	FAdd,
	FSub,
	FMul,
	FDiv,
	FNeg,
	Fmadd,
	Fmsub,
	Fnmadd,
	Fnmsub,
	// Vector SIMD (128-Bit NEON / SSE / AVX)
	VecAdd,
	VecSub,
	VecMul,
	VecDiv,
	VecAnd,
	VecOr,
	VecXor,
	VecMin,
	VecMax,
	VecSqrt,
	VecLoad,
	VecStore,
	VecMov,
	VecFmadd,
	VecFmsub,
	VecFnmadd,
	VecFnmsub,
	VectorExtract,
	VectorInsert,
	VectorBlend,
	VectorZeroExtend
};

enum class IRCondition : uint8_t {
	Equal = 0,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
	Below,
	BelowOrEqual,
	Above,
	AboveOrEqual,
	Overflow,
	NoOverflow
};

struct VirtualReg {
	uint32_t id       = 0;
	DataType type     = DataType::Int64;
	int8_t   phys_pin = -1; // Optional physical register hint (-1 if unpinned)

	bool operator==(const VirtualReg& other) const noexcept {
		return id == other.id && type == other.type;
	}
	bool operator!=(const VirtualReg& other) const noexcept {
		return !(*this == other);
	}
};

class BasicBlock;
class IRInstruction;

struct Value {
	enum class Kind : uint8_t {
		Invalid = 0,
		VReg,
		ImmInt,
		ImmFloat,
		BlockRef,
		MemoryRef
	} kind = Kind::Invalid;

	VirtualReg vreg{};
	int64_t    imm_int = 0;
	double     imm_float = 0.0;
	BasicBlock* block_ref = nullptr;

	struct MemoryLocation {
		VirtualReg base{};
		VirtualReg index{};
		uint8_t    scale = 1;
		int32_t    disp  = 0;
	} mem_ref{};

	static Value MakeVReg(VirtualReg vr) noexcept {
		Value v;
		v.kind = Kind::VReg;
		v.vreg = vr;
		return v;
	}

	static Value MakeImmInt(int64_t val, DataType dt = DataType::Int64) noexcept {
		Value v;
		v.kind = Kind::ImmInt;
		v.imm_int = val;
		v.vreg.type = dt;
		return v;
	}

	static Value MakeImmFloat(double val, DataType dt = DataType::Float64) noexcept {
		Value v;
		v.kind = Kind::ImmFloat;
		v.imm_float = val;
		v.vreg.type = dt;
		return v;
	}

	static Value MakeBlockRef(BasicBlock* bb) noexcept {
		Value v;
		v.kind = Kind::BlockRef;
		v.block_ref = bb;
		return v;
	}

	static Value MakeMemory(VirtualReg base, VirtualReg index, uint8_t scale, int32_t disp) noexcept {
		Value v;
		v.kind = Kind::MemoryRef;
		v.mem_ref = {base, index, scale, disp};
		return v;
	}

	bool IsVReg() const noexcept { return kind == Kind::VReg; }
	bool IsImmInt() const noexcept { return kind == Kind::ImmInt; }
	bool IsImmFloat() const noexcept { return kind == Kind::ImmFloat; }
	bool IsBlockRef() const noexcept { return kind == Kind::BlockRef; }
	bool IsMemoryRef() const noexcept { return kind == Kind::MemoryRef; }
};

struct PhiOperand {
	BasicBlock* pred_block = nullptr;
	Value       val;
};

class IRInstruction {
public:
	IRInstruction(IROpcode op = IROpcode::Nop) : m_opcode(op) {}
	~IRInstruction() = default;

	KYTY_CLASS_NO_COPY(IRInstruction);

	[[nodiscard]] IROpcode GetOpcode() const noexcept { return m_opcode; }
	void SetOpcode(IROpcode op) noexcept { m_opcode = op; }

	[[nodiscard]] VirtualReg GetDst() const noexcept { return m_dst; }
	void SetDst(VirtualReg dst) noexcept { m_dst = dst; m_has_dst = true; }
	[[nodiscard]] bool HasDst() const noexcept { return m_has_dst; }

	[[nodiscard]] const std::vector<Value>& GetOperands() const noexcept { return m_operands; }
	[[nodiscard]] std::vector<Value>& GetOperands() noexcept { return m_operands; }
	void AddOperand(const Value& val) { m_operands.push_back(val); }
	void SetOperand(size_t index, const Value& val) {
		if (index < m_operands.size()) m_operands[index] = val;
	}

	[[nodiscard]] const std::vector<PhiOperand>& GetPhiOperands() const noexcept { return m_phi_operands; }
	void AddPhiOperand(BasicBlock* pred, const Value& val) {
		m_phi_operands.push_back({pred, val});
	}

	[[nodiscard]] IRCondition GetCondition() const noexcept { return m_cond; }
	void SetCondition(IRCondition cond) noexcept { m_cond = cond; }

	[[nodiscard]] uint64_t GetGuestRip() const noexcept { return m_guest_rip; }
	void SetGuestRip(uint64_t rip) noexcept { m_guest_rip = rip; }

	[[nodiscard]] BasicBlock* GetParent() const noexcept { return m_parent; }
	void SetParent(BasicBlock* parent) noexcept { m_parent = parent; }

	[[nodiscard]] bool IsActive() const noexcept { return m_active; }
	void SetActive(bool active) noexcept { m_active = active; }

	[[nodiscard]] bool IsTerminator() const noexcept {
		return m_opcode == IROpcode::Jump || m_opcode == IROpcode::BranchCond ||
		       m_opcode == IROpcode::Return || m_opcode == IROpcode::Switch ||
		       m_opcode == IROpcode::Unreachable;
	}

private:
	IROpcode               m_opcode     = IROpcode::Nop;
	VirtualReg             m_dst{};
	bool                   m_has_dst    = false;
	std::vector<Value>     m_operands;
	std::vector<PhiOperand> m_phi_operands;
	IRCondition            m_cond       = IRCondition::Equal;
	uint64_t               m_guest_rip  = 0;
	BasicBlock*            m_parent     = nullptr;
	bool                   m_active     = true;
};

class BasicBlock {
public:
	explicit BasicBlock(uint32_t id, std::string label = "")
	    : m_id(id), m_label(std::move(label)) {}
	~BasicBlock() = default;

	KYTY_CLASS_NO_COPY(BasicBlock);

	[[nodiscard]] uint32_t GetId() const noexcept { return m_id; }
	[[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

	void AddInstruction(std::unique_ptr<IRInstruction> inst) {
		if (inst) {
			inst->SetParent(this);
			m_instructions.push_back(std::move(inst));
		}
	}

	[[nodiscard]] const std::vector<std::unique_ptr<IRInstruction>>& GetInstructions() const noexcept {
		return m_instructions;
	}
	[[nodiscard]] std::vector<std::unique_ptr<IRInstruction>>& GetInstructions() noexcept {
		return m_instructions;
	}

	void AddPredecessor(BasicBlock* pred) {
		if (pred && !HasPredecessor(pred)) m_predecessors.push_back(pred);
	}
	void RemovePredecessor(BasicBlock* pred) {
		std::erase(m_predecessors, pred);
	}
	[[nodiscard]] bool HasPredecessor(BasicBlock* pred) const noexcept {
		for (auto* p : m_predecessors) if (p == pred) return true;
		return false;
	}
	[[nodiscard]] const std::vector<BasicBlock*>& GetPredecessors() const noexcept { return m_predecessors; }

	void AddSuccessor(BasicBlock* succ) {
		if (succ && !HasSuccessor(succ)) m_successors.push_back(succ);
	}
	void RemoveSuccessor(BasicBlock* succ) {
		std::erase(m_successors, succ);
	}
	[[nodiscard]] bool HasSuccessor(BasicBlock* succ) const noexcept {
		for (auto* s : m_successors) if (s == succ) return true;
		return false;
	}
	[[nodiscard]] const std::vector<BasicBlock*>& GetSuccessors() const noexcept { return m_successors; }

private:
	uint32_t                                     m_id = 0;
	std::string                                  m_label;
	std::vector<std::unique_ptr<IRInstruction>>  m_instructions;
	std::vector<BasicBlock*>                     m_predecessors;
	std::vector<BasicBlock*>                     m_successors;
};

class ControlFlowGraph {
public:
	ControlFlowGraph() = default;
	~ControlFlowGraph() = default;

	KYTY_CLASS_NO_COPY(ControlFlowGraph);

	BasicBlock* CreateBlock(const std::string& label = "") {
		uint32_t id = static_cast<uint32_t>(m_blocks.size());
		std::string block_label = label.empty() ? ("block_" + std::to_string(id)) : label;
		auto block = std::make_unique<BasicBlock>(id, block_label);
		BasicBlock* raw_ptr = block.get();
		m_blocks.push_back(std::move(block));
		if (!m_entry_block) m_entry_block = raw_ptr;
		return raw_ptr;
	}

	[[nodiscard]] BasicBlock* GetEntryBlock() const noexcept { return m_entry_block; }
	void SetEntryBlock(BasicBlock* entry) noexcept { m_entry_block = entry; }

	[[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& GetBlocks() const noexcept { return m_blocks; }

	VirtualReg AllocateVReg(DataType type = DataType::Int64) {
		VirtualReg v;
		v.id = m_next_vreg_id++;
		v.type = type;
		return v;
	}

	void AddEdge(BasicBlock* src, BasicBlock* dst) {
		if (src && dst) {
			src->AddSuccessor(dst);
			dst->AddPredecessor(src);
		}
	}

	[[nodiscard]] std::vector<BasicBlock*> ComputeReversePostOrder() const;

private:
	BasicBlock*                                 m_entry_block = nullptr;
	std::vector<std::unique_ptr<BasicBlock>>     m_blocks;
	uint32_t                                    m_next_vreg_id = 1;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_COMPILER_IR_H
