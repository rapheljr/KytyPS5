// OpenOrbisRelocationTests.cpp
//
// Unit & Integration Tests for OpenOrbis PRX Dynamic Relocation Engine.

#include "loader/openOrbisElfLoader.h"
#include "loader/elf.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Loader;

static void TestProcessRelocations() {
	std::printf("[TEST] OpenOrbisProcessRelocations\n");

	OpenOrbisLoadResult result;
	result.base_vaddr = 0x400000;
	result.image_size = 0x1000;
	result.image_buffer.assign(0x1000, 0);

	// Setup mock segment
	OrbisLoadedSegment seg;
	seg.guest_vaddr = 0x400000;
	seg.host_offset = 0;
	seg.file_size = 0x1000;
	seg.mem_size = 0x1000;
	result.segments.push_back(seg);

	// Build mock synthetic ELF with PT_DYNAMIC and DT_RELA entries
	std::vector<uint8_t> mock_elf(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + sizeof(Elf64_Dyn) * 4 + sizeof(Elf64_Rela) * 2);

	auto* hdr = reinterpret_cast<Elf64_Ehdr*>(mock_elf.data());
	hdr->e_ident[0] = 0x7F;
	hdr->e_ident[1] = 'E';
	hdr->e_ident[2] = 'L';
	hdr->e_ident[3] = 'F';
	hdr->e_ident[4] = ELFCLASS64;
	hdr->e_ident[5] = ELFDATA2LSB;
	hdr->e_type = ET_DYNAMIC;
	hdr->e_machine = EM_X86_64;
	hdr->e_phoff = sizeof(Elf64_Ehdr);
	hdr->e_phnum = 1;

	auto* ph = reinterpret_cast<Elf64_Phdr*>(mock_elf.data() + hdr->e_phoff);
	ph->p_type = PT_DYNAMIC;
	ph->p_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
	ph->p_filesz = sizeof(Elf64_Dyn) * 4;
	ph->p_vaddr = 0x400000 + ph->p_offset;
	ph->p_memsz = ph->p_filesz;

	uint64_t rela_vaddr = 0x400000 + ph->p_offset + ph->p_filesz;
	uint64_t rela_offset = ph->p_offset + ph->p_filesz;

	auto* dyn = reinterpret_cast<Elf64_Dyn*>(mock_elf.data() + ph->p_offset);
	dyn[0].d_tag = 0x07; // DT_RELA
	dyn[0].d_un.d_val = rela_vaddr;
	dyn[1].d_tag = 0x08; // DT_RELASZ
	dyn[1].d_un.d_val = sizeof(Elf64_Rela) * 2;
	dyn[2].d_tag = 0;
	dyn[2].d_un.d_val = 0;

	auto* rela = reinterpret_cast<Elf64_Rela*>(mock_elf.data() + rela_offset);
	// Reloc 1: R_X86_64_RELATIVE at vaddr 0x400100
	rela[0].r_offset = 0x400100;
	rela[0].r_info = 8; // R_X86_64_RELATIVE
	rela[0].r_addend = 0x1234;

	// Reloc 2: R_X86_64_JUMP_SLOT at vaddr 0x400200
	rela[1].r_offset = 0x400200;
	rela[1].r_info = 7; // R_X86_64_JUMP_SLOT
	rela[1].r_addend = 0x5678;

	bool ok = OpenOrbisElfLoader::ProcessRelocations(mock_elf.data(), mock_elf.size(), result);
	if (!ok) {
		std::fprintf(stderr, "FAIL: ProcessRelocations returned false\n");
		std::exit(1);
	}

	if (result.reloc_count != 2 || result.resolved_symbols_count != 2) {
		std::fprintf(stderr, "FAIL: ProcessRelocations count mismatch (reloc=%u, resolved=%u)\n",
		             result.reloc_count, result.resolved_symbols_count);
		std::exit(1);
	}

	uint64_t val1 = 0;
	std::memcpy(&val1, result.image_buffer.data() + (0x400100 - 0x400000), sizeof(uint64_t));
	if (val1 != 0x400000 + 0x1234) {
		std::fprintf(stderr, "FAIL: R_X86_64_RELATIVE patch mismatch (0x%llx vs expected 0x401234)\n", (unsigned long long)val1);
		std::exit(1);
	}

	uint64_t val2 = 0;
	std::memcpy(&val2, result.image_buffer.data() + (0x400200 - 0x400000), sizeof(uint64_t));
	if (val2 != 0x400000 + 0x5678) {
		std::fprintf(stderr, "FAIL: R_X86_64_JUMP_SLOT patch mismatch (0x%llx vs expected 0x405678)\n", (unsigned long long)val2);
		std::exit(1);
	}

	std::printf("  [ OK ] OpenOrbisProcessRelocations\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — OpenOrbis PRX Dynamic Relocation Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestProcessRelocations();

	std::printf("================================================================================\n");
	std::printf("  Results: 1 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
