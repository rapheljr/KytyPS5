#include "graphics/shader/recompiler/emitter/MslEmitter.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "MslEmitterTests: failed: %s\n", text);
		std::abort();
	}
}

void TestComputeShaderEmission() {
	using namespace Libs::Graphics;
	using namespace Libs::Graphics::ShaderRecompiler;

	IR::Program program;
	program.stage = ShaderType::Compute;
	program.wave_size = 64;

	IR::BasicBlock block;
	block.id = 0;

	// v0 = storage_buffers[0]
	IR::Instruction inst1;
	inst1.op = IR::Opcode::BufferLoadDword;
	inst1.dst.kind = IR::OperandKind::Register;
	inst1.dst.reg = IR::Register{IR::RegisterFile::Vector, 0};
	inst1.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, 0};
	inst1.src_count = 1;
	block.instructions.push_back(inst1);

	// v1 = v0 + 42u
	IR::Instruction inst2;
	inst2.op = IR::Opcode::IAddU32;
	inst2.dst.kind = IR::OperandKind::Register;
	inst2.dst.reg = IR::Register{IR::RegisterFile::Vector, 1};
	inst2.src[0] = IR::Operand{IR::OperandKind::Register, IR::Register{IR::RegisterFile::Vector, 0}, 0};
	inst2.src[1] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, 42};
	inst2.src_count = 2;
	block.instructions.push_back(inst2);

	// storage_buffers[4] = v1
	IR::Instruction inst3;
	inst3.op = IR::Opcode::BufferStoreDword;
	inst3.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, 4};
	inst3.src[1] = IR::Operand{IR::OperandKind::Register, IR::Register{IR::RegisterFile::Vector, 1}, 0};
	inst3.src_count = 2;
	block.instructions.push_back(inst3);

	program.blocks.push_back(block);

	IR::ResourceSnapshot resources;
	std::string msl_source;
	std::string error;

	bool ok = Msl::EmitProgram(program, resources, nullptr, nullptr, nullptr, msl_source, &error);
	Check(ok, "Msl::EmitProgram for Compute Shader failed");
	Check(!msl_source.empty(), "MSL source string must not be empty");

	Check(msl_source.find("#include <metal_stdlib>") != std::string::npos, "Missing metal_stdlib header");
	Check(msl_source.find("using namespace metal;") != std::string::npos, "Missing using namespace metal");
	Check(msl_source.find("struct PushConstants") != std::string::npos, "Missing PushConstants struct");
	Check(msl_source.find("kernel void main0") != std::string::npos, "Missing kernel entry point");
	Check(msl_source.find("storage_buffers") != std::string::npos, "Missing storage_buffers binding");

	std::printf("  [OK] Compute Shader Emission Verified:\n");
	std::printf("----------------------------------------------------\n");
	std::printf("%s", msl_source.c_str());
	std::printf("----------------------------------------------------\n");
}

void TestVertexShaderEmission() {
	using namespace Libs::Graphics;
	using namespace Libs::Graphics::ShaderRecompiler;

	IR::Program program;
	program.stage = ShaderType::Vertex;

	IR::StageInput in0;
	in0.kind = IR::StageInputKind::Parameter;
	in0.location = 0;
	program.info.inputs.push_back(in0);

	IR::StageOutput out_pos;
	out_pos.kind = IR::StageOutputKind::Position;
	program.info.outputs.push_back(out_pos);

	IR::StageOutput out_param;
	out_param.kind = IR::StageOutputKind::Parameter;
	out_param.index = 0;
	out_param.location = 0;
	program.info.outputs.push_back(out_param);

	IR::BasicBlock block;
	block.id = 0;

	// ExportPos
	IR::Instruction inst_pos;
	inst_pos.op = IR::Opcode::Export;
	inst_pos.export_info.kind = IR::ExportTargetKind::Position;
	inst_pos.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.0f)};
	inst_pos.src[1] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(2.0f)};
	inst_pos.src[2] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.0f)};
	inst_pos.src[3] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.0f)};
	inst_pos.src_count = 4;
	block.instructions.push_back(inst_pos);

	// ExportParam
	IR::Instruction inst_param;
	inst_param.op = IR::Opcode::Export;
	inst_param.export_info.kind = IR::ExportTargetKind::Parameter;
	inst_param.export_info.target = 0;
	inst_param.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.5f)};
	inst_param.src[1] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.5f)};
	inst_param.src[2] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.5f)};
	inst_param.src[3] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.0f)};
	inst_param.src_count = 4;
	block.instructions.push_back(inst_param);

	program.blocks.push_back(block);

	IR::ResourceSnapshot resources;
	std::string msl_source;
	std::string error;

	bool ok = Msl::EmitProgram(program, resources, nullptr, nullptr, nullptr, msl_source, &error);
	Check(ok, "Msl::EmitProgram for Vertex Shader failed");
	Check(msl_source.find("vertex VertexOutput main0") != std::string::npos, "Missing vertex entry point");
	Check(msl_source.find("out.position = float4") != std::string::npos, "Missing position export");
	Check(msl_source.find("out.param0 = float4") != std::string::npos, "Missing param export");

	std::printf("  [OK] Vertex Shader Emission Verified\n");
}

void TestPixelShaderEmission() {
	using namespace Libs::Graphics;
	using namespace Libs::Graphics::ShaderRecompiler;

	IR::Program program;
	program.stage = ShaderType::Pixel;

	IR::StageOutput out_mrt;
	out_mrt.kind = IR::StageOutputKind::Mrt;
	out_mrt.index = 0;
	program.info.outputs.push_back(out_mrt);

	IR::StageOutput out_depth;
	out_depth.kind = IR::StageOutputKind::Depth;
	program.info.outputs.push_back(out_depth);

	IR::BasicBlock block;
	block.id = 0;

	// ExportMrt
	IR::Instruction inst_mrt;
	inst_mrt.op = IR::Opcode::Export;
	inst_mrt.export_info.kind = IR::ExportTargetKind::Mrt;
	inst_mrt.export_info.target = 0;
	inst_mrt.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.0f)};
	inst_mrt.src[1] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.0f)};
	inst_mrt.src[2] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.0f)};
	inst_mrt.src[3] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.0f)};
	inst_mrt.src_count = 4;
	block.instructions.push_back(inst_mrt);

	// ExportDepth
	IR::Instruction inst_depth;
	inst_depth.op = IR::Opcode::Export;
	inst_depth.export_info.kind = IR::ExportTargetKind::MrtZ;
	inst_depth.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(0.99f)};
	inst_depth.src_count = 1;
	block.instructions.push_back(inst_depth);

	program.blocks.push_back(block);

	IR::ResourceSnapshot resources;
	std::string msl_source;
	std::string error;

	bool ok = Msl::EmitProgram(program, resources, nullptr, nullptr, nullptr, msl_source, &error);
	Check(ok, "Msl::EmitProgram for Pixel Shader failed");
	Check(msl_source.find("fragment FragmentOutput main0") != std::string::npos, "Missing fragment entry point");
	Check(msl_source.find("out.color0 = float4") != std::string::npos, "Missing color export");
	Check(msl_source.find("out.depth =") != std::string::npos, "Missing depth export");

	std::printf("  [OK] Pixel/Fragment Shader Emission Verified\n");
}

void TestTypeTranslationAndBitcasts() {
	using namespace Libs::Graphics;
	using namespace Libs::Graphics::ShaderRecompiler;

	IR::Program program;
	program.stage = ShaderType::Compute;

	IR::BasicBlock block;
	block.id = 0;

	// ConvertF32ToU32
	IR::Instruction i1;
	i1.op = IR::Opcode::ConvertF32ToU32;
	i1.dst.kind = IR::OperandKind::Register;
	i1.dst.reg = IR::Register{IR::RegisterFile::Vector, 0};
	i1.src[0] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, 0x3f800000u};
	i1.src_count = 1;
	block.instructions.push_back(i1);

	// ConvertU32ToF32
	IR::Instruction i2;
	i2.op = IR::Opcode::ConvertU32ToF32;
	i2.dst.kind = IR::OperandKind::Register;
	i2.dst.reg = IR::Register{IR::RegisterFile::Vector, 1};
	i2.src[0] = IR::Operand{IR::OperandKind::Register, IR::Register{IR::RegisterFile::Vector, 0}, 0};
	i2.src_count = 1;
	block.instructions.push_back(i2);

	program.blocks.push_back(block);

	IR::ResourceSnapshot resources;
	std::string msl_source;
	std::string error;

	bool ok = Msl::EmitProgram(program, resources, nullptr, nullptr, nullptr, msl_source, &error);
	Check(ok, "Msl::EmitProgram for bitcast/conversion test failed");
	Check(msl_source.find("as_type<float>") != std::string::npos, "Missing as_type<float>");
	Check(msl_source.find("static_cast<uint>") != std::string::npos, "Missing static_cast<uint>");

	std::printf("  [OK] Type Translation & Bitcasts Verified\n");
}

void BenchmarkMslEmissionTime() {
	using namespace Libs::Graphics;
	using namespace Libs::Graphics::ShaderRecompiler;

	IR::Program program;
	program.stage = ShaderType::Compute;

	IR::BasicBlock block;
	block.id = 0;

	for (size_t i = 0; i < 50; ++i) {
		IR::Instruction inst;
		inst.op = IR::Opcode::FMadF32;
		inst.dst.kind = IR::OperandKind::Register;
		inst.dst.reg = IR::Register{IR::RegisterFile::Vector, static_cast<uint32_t>(i % 32)};
		inst.src[0] = IR::Operand{IR::OperandKind::Register, IR::Register{IR::RegisterFile::Vector, static_cast<uint32_t>((i + 1) % 32)}, 0};
		inst.src[1] = IR::Operand{IR::OperandKind::Register, IR::Register{IR::RegisterFile::Vector, static_cast<uint32_t>((i + 2) % 32)}, 0};
		inst.src[2] = IR::Operand{IR::OperandKind::ImmediateU32, IR::Register{}, std::bit_cast<uint32_t>(1.5f)};
		inst.src_count = 3;
		block.instructions.push_back(inst);
	}
	program.blocks.push_back(block);

	IR::ResourceSnapshot resources;
	static constexpr size_t ITERS = 1000;

	const auto t0 = std::chrono::high_resolution_clock::now();
	for (size_t iter = 0; iter < ITERS; ++iter) {
		std::string msl_source;
		std::string error;
		bool ok = Msl::EmitProgram(program, resources, nullptr, nullptr, nullptr, msl_source, &error);
		Check(ok, "Msl::EmitProgram benchmark failed");
	}
	const auto t1 = std::chrono::high_resolution_clock::now();

	double total_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	double ms_per_shader = total_ms / static_cast<double>(ITERS);
	double ns_per_inst = (ms_per_shader * 1e6) / 50.0;

	std::printf("  [Bench] MSL Shader Emission Latency: %.3f µs/shader (%.2f ns/instruction, Total: %.2f ms over %zu iterations)\n",
	            ms_per_shader * 1000.0, ns_per_inst, total_ms, ITERS);
}

void DocumentUnsupportedInstructions() {
	std::printf("\n--- MSL Emitter Unsupported / Extended Instruction Documentation ---\n");
	std::printf("  - Wave / Subgroup Matrix Extensions: Hardware-specific RDNA WMMA matrix ops fallback to scalar float calculations.\n");
	std::printf("  - Custom Raytracing Traversal: DXR/GCN custom ray query acceleration structures map to Metal Raytracing API.\n");
	std::printf("  - Sparse Texture Residue: Sparse texture residency query instructions return true (fully resident memory).\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Native MSL Emitter Unit & Benchmark Tests   \n");
	std::printf("====================================================\n\n");

	TestComputeShaderEmission();
	TestVertexShaderEmission();
	TestPixelShaderEmission();
	TestTypeTranslationAndBitcasts();

	std::printf("\n");
	BenchmarkMslEmissionTime();

	DocumentUnsupportedInstructions();

	std::printf("\nMslEmitterTests: PASSED\n");
	return 0;
}
