// OpenOrbisRelocationTests.cpp
//
// Unit & Integration Tests for OpenOrbis PRX Dynamic Relocation Engine.

#include "loader/openOrbisElfLoader.h"
#include "kernel/openOrbisSubsystems.h"
#include "loader/recompiler/jitTelemetryCollector.h"
#include "loader/elf.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Loader;


static void Check(bool condition, const char* msg) {
	if (!condition) {
		std::fprintf(stderr, "ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

static void TestProcessRelocations_Basic() {
	std::printf("[TEST] Relocations_Basic (Relative & Addend)\n");

	OpenOrbisLoadResult result;
	result.base_vaddr = 0x400000;
	result.image_size = 0x1000;
	result.image_buffer.assign(0x1000, 0);

	OrbisLoadedSegment seg;
	seg.guest_vaddr = 0x400000;
	seg.host_offset = 0;
	seg.file_size = 0x1000;
	seg.mem_size = 0x1000;
	result.segments.push_back(seg);

	std::vector<uint8_t> mock_elf(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + sizeof(Elf64_Dyn) * 4 + sizeof(Elf64_Rela) * 2);

	auto* hdr = reinterpret_cast<Elf64_Ehdr*>(mock_elf.data());
	hdr->e_ident[0] = 0x7F;
	hdr->e_ident[1] = 'E';
	hdr->e_ident[2] = 'L';
	hdr->e_ident[3] = 'F';
	hdr->e_ident[4] = 2; // ELFCLASS64
	hdr->e_ident[5] = 1; // ELFDATA2LSB
	hdr->e_type = 3; // ET_DYN
	hdr->e_machine = 62; // EM_X86_64
	hdr->e_phoff = sizeof(Elf64_Ehdr);
	hdr->e_phnum = 1;

	auto* ph = reinterpret_cast<Elf64_Phdr*>(mock_elf.data() + hdr->e_phoff);
	ph->p_type = 2; // PT_DYNAMIC
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
	Check(ok, "ProcessRelocations failed");
	Check(result.reloc_count == 2, "reloc_count != 2");
	Check(result.resolved_symbols_count == 2, "resolved_symbols_count != 2");

	uint64_t val1 = 0;
	std::memcpy(&val1, result.image_buffer.data() + (0x400100 - 0x400000), sizeof(uint64_t));
	Check(val1 == 0x400000 + 0x1234, "R_X86_64_RELATIVE value mismatch");

	uint64_t val2 = 0;
	std::memcpy(&val2, result.image_buffer.data() + (0x400200 - 0x400000), sizeof(uint64_t));
	Check(val2 == 0x400000 + 0x5678, "R_X86_64_JUMP_SLOT value mismatch");

	std::printf("  [ OK ] Relocations_Basic\n");
}

static void TestProcessRelocations_DynamicSymbolResolution() {
	std::printf("[TEST] Relocations_DynamicSymbolResolution (DynSym / DynStr Parsing)\n");

	OpenOrbisLoadResult result;
	result.base_vaddr = 0x600000;
	result.image_size = 0x4000;
	result.image_buffer.assign(0x4000, 0);

	OrbisLoadedSegment seg;
	seg.guest_vaddr = 0x600000;
	seg.host_offset = 0;
	seg.file_size = 0x4000;
	seg.mem_size = 0x4000;
	result.segments.push_back(seg);

	// String table: "\0my_global_fn\0my_global_var\0"
	const char strtab[] = "\0my_global_fn\0my_global_var\0";
	size_t strtab_sz = sizeof(strtab);

	// Symbol table: 3 symbols (0: NULL, 1: my_global_fn, 2: my_global_var)
	Elf64_Sym syms[3]{};
	// sym 1: defined function at offset 0x1000
	syms[1].st_name = 1; // "my_global_fn"
	syms[1].st_info = 0x12; // STB_GLOBAL, STT_FUNC
	syms[1].st_shndx = 1; // defined!
	syms[1].st_value = 0x1000;
	syms[1].st_size = 64;

	// sym 2: defined variable at offset 0x2000
	syms[2].st_name = 14; // "my_global_var"
	syms[2].st_info = 0x11; // STB_GLOBAL, STT_OBJECT
	syms[2].st_shndx = 2; // defined!
	syms[2].st_value = 0x2000;
	syms[2].st_size = 8;

	// Relocations:
	// Reloc 1: JUMP_SLOT targeting sym 1 at GOT slot 0x600800
	// Reloc 2: GLOB_DAT targeting sym 2 at GOT slot 0x600808
	Elf64_Rela relas[2]{};
	relas[0].r_offset = 0x600800;
	relas[0].r_info = (static_cast<uint64_t>(1) << 32) | 7; // sym 1, R_X86_64_JUMP_SLOT
	relas[0].r_addend = 0;

	relas[1].r_offset = 0x600808;
	relas[1].r_info = (static_cast<uint64_t>(2) << 32) | 6; // sym 2, R_X86_64_GLOB_DAT
	relas[1].r_addend = 4;

	// Build ELF image
	size_t offset_ehdr = 0;
	size_t offset_phdr = sizeof(Elf64_Ehdr);
	size_t offset_dyn = offset_phdr + sizeof(Elf64_Phdr);
	size_t offset_strtab = offset_dyn + sizeof(Elf64_Dyn) * 10;
	size_t offset_symtab = offset_strtab + strtab_sz;
	size_t offset_rela = offset_symtab + sizeof(syms);
	size_t total_sz = offset_rela + sizeof(relas);

	std::vector<uint8_t> elf_data(total_sz);
	auto* hdr = reinterpret_cast<Elf64_Ehdr*>(elf_data.data() + offset_ehdr);
	hdr->e_ident[0] = 0x7F; hdr->e_ident[1] = 'E'; hdr->e_ident[2] = 'L'; hdr->e_ident[3] = 'F';
	hdr->e_ident[4] = 2; hdr->e_ident[5] = 1; hdr->e_type = 3; hdr->e_machine = 62;
	hdr->e_phoff = offset_phdr;
	hdr->e_phnum = 1;

	auto* ph = reinterpret_cast<Elf64_Phdr*>(elf_data.data() + offset_phdr);
	ph->p_type = 2; // PT_DYNAMIC
	ph->p_offset = offset_dyn;
	ph->p_filesz = sizeof(Elf64_Dyn) * 10;
	ph->p_vaddr = 0x600000 + offset_dyn;
	ph->p_memsz = ph->p_filesz;

	std::memcpy(elf_data.data() + offset_strtab, strtab, strtab_sz);
	std::memcpy(elf_data.data() + offset_symtab, syms, sizeof(syms));
	std::memcpy(elf_data.data() + offset_rela, relas, sizeof(relas));

	auto* dyn = reinterpret_cast<Elf64_Dyn*>(elf_data.data() + offset_dyn);
	dyn[0].d_tag = 0x07; dyn[0].d_un.d_val = 0x600000 + offset_rela; // DT_RELA
	dyn[1].d_tag = 0x08; dyn[1].d_un.d_val = sizeof(relas);          // DT_RELASZ
	dyn[2].d_tag = 0x06; dyn[2].d_un.d_val = 0x600000 + offset_symtab; // DT_SYMTAB
	dyn[3].d_tag = 0x0B; dyn[3].d_un.d_val = sizeof(Elf64_Sym);       // DT_SYMENT
	dyn[4].d_tag = 0x05; dyn[4].d_un.d_val = 0x600000 + offset_strtab; // DT_STRTAB
	dyn[5].d_tag = 0x0A; dyn[5].d_un.d_val = strtab_sz;               // DT_STRSZ
	dyn[6].d_tag = 0;    dyn[6].d_un.d_val = 0;

	bool ok = OpenOrbisElfLoader::ProcessRelocations(elf_data.data(), elf_data.size(), result);
	Check(ok, "ProcessRelocations with symbol table failed");
	Check(result.symbols.size() >= 2, "Failed to extract dynamic symbols");
	Check(result.reloc_count == 2, "reloc_count != 2");
	Check(result.resolved_symbols_count == 2, "resolved_symbols_count != 2");

	// Verify GOT slot 0x600800 got 0x600000 + 0x1000 = 0x601000
	uint64_t got1 = 0;
	std::memcpy(&got1, result.image_buffer.data() + (0x600800 - 0x600000), sizeof(uint64_t));
	Check(got1 == 0x601000, "GOT slot 1 resolved address mismatch (expected 0x601000)");

	// Verify GOT slot 0x600808 got 0x600000 + 0x2000 + 4 = 0x602004
	uint64_t got2 = 0;
	std::memcpy(&got2, result.image_buffer.data() + (0x600808 - 0x600000), sizeof(uint64_t));
	Check(got2 == 0x602004, "GOT slot 2 resolved address mismatch (expected 0x602004)");

	std::printf("  [ OK ] Relocations_DynamicSymbolResolution\n");
}

static void TestProcessRelocations_SubsystemStubBinding() {
	std::printf("[TEST] Relocations_SubsystemStubBinding (Host Subsystem Resolver)\n");

	Recompiler::JitTelemetryCollector telemetry;
	Kernel::OpenOrbisSubsystemHub hub(telemetry);
	hub.RegisterAll();

	OpenOrbisLoadResult result;
	result.base_vaddr = 0x400000;
	result.image_size = 0x3000;
	result.image_buffer.assign(0x3000, 0);

	OrbisLoadedSegment seg;
	seg.guest_vaddr = 0x400000;
	seg.host_offset = 0;
	seg.file_size = 0x3000;
	seg.mem_size = 0x3000;
	result.segments.push_back(seg);

	// String table: "\0sceKernelOpen\0scePthreadCreate\0unknownSceFn\0"
	const char strtab[] = "\0sceKernelOpen\0scePthreadCreate\0unknownSceFn\0";
	size_t strtab_sz = sizeof(strtab);

	// Symbol table:
	// sym 1: undefined import "sceKernelOpen" (shndx = 0)
	// sym 2: undefined import "scePthreadCreate" (shndx = 0)
	// sym 3: undefined import "unknownSceFn" (shndx = 0)
	Elf64_Sym syms[4]{};
	syms[1].st_name = 1; // "sceKernelOpen"
	syms[1].st_shndx = 0; // SHN_UNDEF

	syms[2].st_name = 15; // "scePthreadCreate"
	syms[2].st_shndx = 0; // SHN_UNDEF

	syms[3].st_name = 32; // "unknownSceFn"
	syms[3].st_shndx = 0; // SHN_UNDEF

	Elf64_Rela relas[3]{};
	relas[0].r_offset = 0x400500;
	relas[0].r_info = (static_cast<uint64_t>(1) << 32) | 7; // sym 1, R_X86_64_JUMP_SLOT

	relas[1].r_offset = 0x400508;
	relas[1].r_info = (static_cast<uint64_t>(2) << 32) | 7; // sym 2, R_X86_64_JUMP_SLOT

	relas[2].r_offset = 0x400510;
	relas[2].r_info = (static_cast<uint64_t>(3) << 32) | 7; // sym 3, R_X86_64_JUMP_SLOT

	size_t offset_ehdr = 0;
	size_t offset_phdr = sizeof(Elf64_Ehdr);
	size_t offset_dyn = offset_phdr + sizeof(Elf64_Phdr);
	size_t offset_strtab = offset_dyn + sizeof(Elf64_Dyn) * 10;
	size_t offset_symtab = offset_strtab + strtab_sz;
	size_t offset_rela = offset_symtab + sizeof(syms);
	size_t total_sz = offset_rela + sizeof(relas);

	std::vector<uint8_t> elf_data(total_sz);
	auto* hdr = reinterpret_cast<Elf64_Ehdr*>(elf_data.data() + offset_ehdr);
	hdr->e_ident[0] = 0x7F; hdr->e_ident[1] = 'E'; hdr->e_ident[2] = 'L'; hdr->e_ident[3] = 'F';
	hdr->e_ident[4] = 2; hdr->e_ident[5] = 1; hdr->e_type = 3; hdr->e_machine = 62;
	hdr->e_phoff = offset_phdr;
	hdr->e_phnum = 1;

	auto* ph = reinterpret_cast<Elf64_Phdr*>(elf_data.data() + offset_phdr);
	ph->p_type = 2; // PT_DYNAMIC
	ph->p_offset = offset_dyn;
	ph->p_filesz = sizeof(Elf64_Dyn) * 10;
	ph->p_vaddr = 0x400000 + offset_dyn;
	ph->p_memsz = ph->p_filesz;

	std::memcpy(elf_data.data() + offset_strtab, strtab, strtab_sz);
	std::memcpy(elf_data.data() + offset_symtab, syms, sizeof(syms));
	std::memcpy(elf_data.data() + offset_rela, relas, sizeof(relas));

	auto* dyn = reinterpret_cast<Elf64_Dyn*>(elf_data.data() + offset_dyn);
	dyn[0].d_tag = 0x07; dyn[0].d_un.d_val = 0x400000 + offset_rela; // DT_RELA
	dyn[1].d_tag = 0x08; dyn[1].d_un.d_val = sizeof(relas);          // DT_RELASZ
	dyn[2].d_tag = 0x06; dyn[2].d_un.d_val = 0x400000 + offset_symtab; // DT_SYMTAB
	dyn[3].d_tag = 0x0B; dyn[3].d_un.d_val = sizeof(Elf64_Sym);       // DT_SYMENT
	dyn[4].d_tag = 0x05; dyn[4].d_un.d_val = 0x400000 + offset_strtab; // DT_STRTAB
	dyn[5].d_tag = 0x0A; dyn[5].d_un.d_val = strtab_sz;               // DT_STRSZ
	dyn[6].d_tag = 0;    dyn[6].d_un.d_val = 0;

	auto resolver = hub.CreateSymbolResolver(0x80000000);
	bool ok = OpenOrbisElfLoader::ProcessRelocations(elf_data.data(), elf_data.size(), result, resolver);
	Check(ok, "ProcessRelocations with hub resolver failed");

	// 2 known symbols resolved, 1 unknown tracked
	Check(result.resolved_symbols_count == 2, "resolved_symbols_count != 2");
	Check(result.unresolved_symbols.size() == 1, "unresolved_symbols count != 1");
	Check(result.unresolved_symbols[0] == "unknownSceFn", "unresolved symbol name mismatch");

	uint64_t stub1 = 0;
	std::memcpy(&stub1, result.image_buffer.data() + (0x400500 - 0x400000), sizeof(uint64_t));
	Check(stub1 >= 0x80000000, "sceKernelOpen stub address not in 0x80000000 range");

	uint64_t stub2 = 0;
	std::memcpy(&stub2, result.image_buffer.data() + (0x400508 - 0x400000), sizeof(uint64_t));
	Check(stub2 >= 0x80000000, "scePthreadCreate stub address not in 0x80000000 range");
	Check(stub1 != stub2, "Stub addresses must be unique per symbol");

	std::printf("  sceKernelOpen Stub VA     = 0x%llX\n", (unsigned long long)stub1);
	std::printf("  scePthreadCreate Stub VA = 0x%llX\n", (unsigned long long)stub2);
	std::printf("  [ OK ] Relocations_SubsystemStubBinding\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — OpenOrbis PRX Dynamic Relocation Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestProcessRelocations_Basic();
	TestProcessRelocations_DynamicSymbolResolution();
	TestProcessRelocations_SubsystemStubBinding();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
