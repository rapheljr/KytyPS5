// Pm4CommandProcessorTests.cpp
//
// Unit, stress, disassembler, translation, and benchmark test suite for Phase K:
// PS5 GPU Command Processor.

#include "graphics/guest_gpu/command_processor/pm4CommandList.h"
#include "graphics/guest_gpu/command_processor/pm4Disassembler.h"
#include "graphics/guest_gpu/command_processor/pm4Parser.h"
#include "graphics/guest_gpu/command_processor/pm4Translator.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"

#include "SDL.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

namespace Pm4 = Libs::Graphics::Pm4;

void Check(bool value, const char* text) {

	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

// ─── 1. Ring Buffer Parser & Decoder Tests ────────────────────────────────────

void TestPm4ParserPacketDecoding() {
	std::printf("  [Test 1] PM4 Ring Buffer Parser & Packet Decoding...\n");

	using namespace Libs::Graphics::Pm4;

	std::vector<uint32_t> stream;
	// Packet 1: IT_DRAW_INDEX_2 (CountDW: 4 dwords total -> header + 3 payload)
	stream.push_back(KYTY_PM4(4, IT_DRAW_INDEX_2, 0));
	stream.push_back(100); // Index count
	stream.push_back(0);   // First vertex
	stream.push_back(0);   // Index type

	// Packet 2: IT_DISPATCH_DIRECT (CountDW: 4 dwords -> header + 3 payload)
	stream.push_back(KYTY_PM4(4, IT_DISPATCH_DIRECT, 0));
	stream.push_back(16); // Group X
	stream.push_back(16); // Group Y
	stream.push_back(1);  // Group Z

	// Packet 3: IT_CP_DMA (CountDW: 6 dwords)
	stream.push_back(KYTY_PM4(6, IT_CP_DMA, 0));
	stream.push_back(0x1000); // Src low
	stream.push_back(0x0000); // Src high
	stream.push_back(0x2000); // Dst low
	stream.push_back(0x0000); // Dst high
	stream.push_back(4096);   // Num bytes

	Pm4RingBufferParser parser;
	uint32_t packets_received = 0;
	parser.SetPacketCallback([&packets_received](const DecodedPacket& packet) {
		packets_received++;
		if (packets_received == 1) {
			Check(packet.opcode == IT_DRAW_INDEX_2, "Packet 1 opcode mismatch");
			Check(std::holds_alternative<DrawPacket>(packet.data), "Packet 1 data type mismatch");
			auto draw = std::get<DrawPacket>(packet.data);
			Check(draw.index_count == 100, "Draw index count mismatch");
		} else if (packets_received == 2) {
			Check(packet.opcode == IT_DISPATCH_DIRECT, "Packet 2 opcode mismatch");
			Check(std::holds_alternative<DispatchPacket>(packet.data), "Packet 2 data type mismatch");
			auto dispatch = std::get<DispatchPacket>(packet.data);
			Check(dispatch.group_x == 16 && dispatch.group_y == 16, "Dispatch group size mismatch");
		} else if (packets_received == 3) {
			Check(packet.opcode == IT_CP_DMA, "Packet 3 opcode mismatch");
			Check(std::holds_alternative<DmaCopyPacket>(packet.data), "Packet 3 data type mismatch");
			auto dma = std::get<DmaCopyPacket>(packet.data);
			Check(dma.src_addr == 0x1000 && dma.dst_addr == 0x2000 && dma.num_bytes == 4096, "DMA address/size mismatch");
		}
	});

	bool ok = parser.ParseStream(stream.data(), stream.size());
	Check(ok, "Parser stream parse failed");
	Check(packets_received == 3, "Received packet count mismatch");
	Check(parser.GetStats().packets_parsed == 3, "Parsed packet count stat mismatch");

	std::printf("  [OK] Test 1: PM4 Ring Buffer Parser & Packet Decoding\n");
}

// ─── 2. Disassembler & Validation Tests ─────────────────────────────────────

void TestPm4DisassemblerAndValidation() {
	std::printf("  [Test 2] PM4 Disassembler & Validation Layer...\n");

	using namespace Libs::Graphics::Pm4;

	std::vector<uint32_t> stream;
	stream.push_back(KYTY_PM4(4, IT_DRAW_INDEX_2, 0));
	stream.push_back(60);
	stream.push_back(0);
	stream.push_back(0);

	std::string disasm = Pm4Disassembler::DisassembleStream(stream.data(), stream.size());
	Check(!disasm.empty(), "Disassembly output empty");
	Check(disasm.find("IT_DRAW_INDEX_2") != std::string::npos, "Opcode string missing in disassembly");

	// Stream validation test
	auto issues = Pm4Disassembler::ValidateStream(stream.data(), stream.size());
	Check(issues.empty(), "Valid stream reported validation errors");

	// Invalid stream validation test
	std::vector<uint32_t> invalid_stream;
	invalid_stream.push_back(KYTY_PM4(10, IT_DRAW_INDEX_2, 0)); // Claims 10 dwords, only provides 1
	auto bad_issues = Pm4Disassembler::ValidateStream(invalid_stream.data(), invalid_stream.size());
	Check(!bad_issues.empty(), "Invalid stream failed to trigger validation error");

	std::printf("  [OK] Test 2: PM4 Disassembler & Validation Layer\n");
}

// ─── 3. Backend-Independent Command List Tests ────────────────────────────────

void TestPm4CommandListRecording() {
	std::printf("  [Test 3] Backend-Independent Command List Recording...\n");

	using namespace Libs::Graphics::Pm4;

	Pm4CommandList cmd_list;
	cmd_list.RecordDrawNonIndexed(36, 1, 0, 0);
	cmd_list.RecordDrawIndexed(120, 1, 0, 0, 0, 0x4000, 0);
	cmd_list.RecordDispatch(8, 8, 1);
	cmd_list.RecordDmaCopy(0x1000, 0x5000, 1024);

	float clear_color[4] = {0.1f, 0.2f, 0.3f, 1.0f};
	cmd_list.RecordClear(clear_color, 1.0f, 0, true, true, false);
	cmd_list.RecordBarrier(true, true, true);

	Check(cmd_list.GetCommandCount() == 6, "Command count mismatch");

	Pm4CommandList appended_list;
	appended_list.Append(cmd_list);
	Check(appended_list.GetCommandCount() == 6, "Appended command count mismatch");

	std::printf("  [OK] Test 3: Backend-Independent Command List Recording\n");
}

// ─── 4. Hardware Translation Layer Tests ────────────────────────────────────

void TestPm4TranslatorExecution() {
	std::printf("  [Test 4] Backend Hardware Translation Layer...\n");

	using namespace Libs::Graphics::Pm4;

#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");

	Pm4CommandList cmd_list;
	cmd_list.RecordDrawNonIndexed(100, 1, 0, 0);
	cmd_list.RecordDispatch(16, 16, 1);
	cmd_list.RecordDmaCopy(0x1000, 0x2000, 512);

	uint64_t timestamp_val = 0;
	cmd_list.RecordTimestampQuery(reinterpret_cast<uint64_t>(&timestamp_val));

	Pm4Translator translator(&metal_backend);
	bool success = translator.TranslateAndExecute(cmd_list);
	Check(success, "Translator execution failed");
	Check(translator.GetStats().commands_translated == 4, "Translated count stat mismatch");
	Check(timestamp_val > 0, "Timestamp query address not written by translator");

	metal_backend.Shutdown();
#endif

	std::printf("  [OK] Test 4: Backend Hardware Translation Layer\n");
}

// ─── 5. Multi-Threaded Command Recording Stress Test ─────────────────────────

void TestMultiThreadedCommandRecording() {
	std::printf("  [Test 5] Multi-Threaded Command Recording Stress Test (8 threads)...\n");

	using namespace Libs::Graphics::Pm4;

	constexpr int kNumThreads = 8;
	constexpr int kCommandsPerThread = 10000;

	std::vector<std::thread> threads;
	std::vector<Pm4CommandList> thread_command_lists(kNumThreads);

	for (int i = 0; i < kNumThreads; ++i) {
		threads.emplace_back([i, &thread_command_lists]() {
			auto& cmd_list = thread_command_lists[i];
			cmd_list.Reserve(kCommandsPerThread);
			for (int j = 0; j < kCommandsPerThread; ++j) {
				cmd_list.RecordDrawNonIndexed(36, 1, 0, 0);
				cmd_list.RecordDispatch(4, 4, 1);
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	Pm4CommandList master_list;
	for (int i = 0; i < kNumThreads; ++i) {
		Check(thread_command_lists[i].GetCommandCount() == static_cast<size_t>(kCommandsPerThread * 2), "Thread command count mismatch");
		master_list.Append(thread_command_lists[i]);
	}

	Check(master_list.GetCommandCount() == static_cast<size_t>(kNumThreads * kCommandsPerThread * 2), "Master combined command count mismatch");

	std::printf("  [OK] Test 5: Multi-Threaded Command Recording Stress Test\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkPm4CommandProcessor() {
	std::printf("\n--- Phase K Benchmarks ---\n");

	using namespace Libs::Graphics::Pm4;

	// 1. PM4 Ring Buffer Parsing Latency Benchmark
	std::vector<uint32_t> stream;
	constexpr int kPacketBatch = 50000;
	stream.reserve(kPacketBatch * 4);

	for (int i = 0; i < kPacketBatch; ++i) {
		stream.push_back(KYTY_PM4(4, IT_DRAW_INDEX_2, 0));
		stream.push_back(100);
		stream.push_back(0);
		stream.push_back(0);
	}

	Pm4RingBufferParser parser;
	uint64_t callback_count = 0;
	parser.SetPacketCallback([&callback_count](const DecodedPacket&) {
		callback_count++;
	});

	auto t0 = std::chrono::high_resolution_clock::now();
	parser.ParseStream(stream.data(), stream.size());
	auto t1 = std::chrono::high_resolution_clock::now();

	double parse_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kPacketBatch;
	double parse_throughput = kPacketBatch / std::chrono::duration<double>(t1 - t0).count();
	std::printf("  [Bench] PM4 Packet Parse Latency: %.2f ns/packet (Throughput: %.2f M packets/sec)\n",
	           parse_dt_ns, parse_throughput / 1e6);

	// 2. Command List Recording Latency Benchmark
	Pm4CommandList bench_list;
	constexpr int kRecordOps = 100000;
	bench_list.Reserve(kRecordOps);

	t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kRecordOps; ++i) {
		bench_list.RecordDrawNonIndexed(36, 1, 0, 0);
	}
	t1 = std::chrono::high_resolution_clock::now();

	double record_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kRecordOps;
	double record_throughput = kRecordOps / std::chrono::duration<double>(t1 - t0).count();
	std::printf("  [Bench] Command List Recording Latency: %.2f ns/command (Throughput: %.2f M cmds/sec)\n",
	           record_dt_ns, record_throughput / 1e6);
}

} // namespace

int main() {
#if defined(__APPLE__)
	SDL_Init(SDL_INIT_VIDEO);
#endif

	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase K: PS5 GPU Command Processor          \n");
	std::printf("====================================================\n\n");

	TestPm4ParserPacketDecoding();
	TestPm4DisassemblerAndValidation();
	TestPm4CommandListRecording();
	TestPm4TranslatorExecution();
	TestMultiThreadedCommandRecording();

	BenchmarkPm4CommandProcessor();

#if defined(__APPLE__)
	SDL_Quit();
#endif

	std::printf("\nPm4CommandProcessorTests: ALL PASSED\n");
	return 0;
}
