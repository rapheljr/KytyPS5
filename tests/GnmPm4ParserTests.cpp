// GnmPm4ParserTests.cpp
//
// Unit & Integration Tests for GNM PM4 Packet Parser & Draw Command Extensions.

#include "graphics/guest_gpu/command_processor/pm4Parser.h"
#include "graphics/guest_gpu/pm4.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Libs::Graphics::Pm4;

static void TestParseDrawIndexAuto() {
	std::printf("[TEST] GnmPm4ParseDrawIndexAuto\n");

	uint32_t payload[2] = {1024, 0};
	DrawPacket draw{};

	bool ok = Pm4RingBufferParser::ParseDrawIndexAuto(payload, 2, draw);
	if (!ok) {
		std::fprintf(stderr, "FAIL: ParseDrawIndexAuto returned false\n");
		std::exit(1);
	}

	if (draw.vertex_count != 1024 || draw.indexed) {
		std::fprintf(stderr, "FAIL: ParseDrawIndexAuto attribute mismatch (count=%u, indexed=%d)\n",
		             draw.vertex_count, draw.indexed);
		std::exit(1);
	}

	std::printf("  [ OK ] GnmPm4ParseDrawIndexAuto\n");
}

static void TestParseDrawIndex() {
	std::printf("[TEST] GnmPm4ParseDrawIndex\n");

	uint32_t payload[3] = {512, 0x1000, 0x8000};
	DrawPacket draw{};

	bool ok = Pm4RingBufferParser::ParseDrawIndex(payload, 3, draw);
	if (!ok) {
		std::fprintf(stderr, "FAIL: ParseDrawIndex returned false\n");
		std::exit(1);
	}

	uint64_t expected_gpu_addr = (static_cast<uint64_t>(0x8000) << 32u) | 0x1000;
	if (draw.index_count != 512 || !draw.indexed || draw.index_gpu_addr != expected_gpu_addr) {
		std::fprintf(stderr, "FAIL: ParseDrawIndex attribute mismatch (count=%u, gpu_addr=0x%llx)\n",
		             draw.index_count, (unsigned long long)draw.index_gpu_addr);
		std::exit(1);
	}

	std::printf("  [ OK ] GnmPm4ParseDrawIndex\n");
}

static void TestPm4RingBufferParserStream() {
	std::printf("[TEST] Pm4RingBufferParserStream\n");

	// Build PM4 Type-3 stream: IT_DRAW_INDEX_AUTO packet
	// Header: type=3, count_dw=3 (count field = 3-2 = 1), opcode=IT_DRAW_INDEX_AUTO (0x2D)
	uint32_t count_field = 1;
	uint32_t opcode = IT_DRAW_INDEX_AUTO;
	uint32_t header_dw = (3u << 30u) | (count_field << 16u) | (opcode << 8u);

	std::vector<uint32_t> stream = {header_dw, 256, 0};

	Pm4RingBufferParser parser;
	uint32_t draw_count = 0;

	parser.SetPacketCallback([&](const DecodedPacket& pkt) {
		if (std::holds_alternative<DrawPacket>(pkt.data)) {
			const auto& draw = std::get<DrawPacket>(pkt.data);
			if (draw.vertex_count == 256) {
				draw_count++;
			}
		}
	});

	bool ok = parser.ParseStream(stream.data(), stream.size());
	if (!ok || draw_count != 1) {
		std::fprintf(stderr, "FAIL: Pm4RingBufferParser failed stream parsing (draw_cnt=%u)\n", draw_count);
		std::exit(1);
	}

	std::printf("  [ OK ] Pm4RingBufferParserStream\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — GNM PM4 Packet Parser Unit & Integration Test Suite\n");
	std::printf("================================================================================\n");

	TestParseDrawIndexAuto();
	TestParseDrawIndex();
	TestPm4RingBufferParserStream();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
