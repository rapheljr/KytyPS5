// openOrbisElfLoader.cpp
//
// OpenOrbis / PS5 Homebrew ELF64 Loader Implementation.

#include "loader/openOrbisElfLoader.h"

#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>

// ELF64 structures (self-contained to avoid pulling in host elf.h)
namespace {

// ELF magic
static constexpr uint8_t ELFMAG0 = 0x7F;
static constexpr uint8_t ELFMAG1 = 'E';
static constexpr uint8_t ELFMAG2 = 'L';
static constexpr uint8_t ELFMAG3 = 'F';

static constexpr uint8_t  ELFCLASS64   = 2;
static constexpr uint8_t  ELFDATA2LSB  = 1;  // Little-endian
static constexpr uint16_t ET_EXEC      = 2;
static constexpr uint16_t ET_DYN       = 3;
static constexpr uint16_t EM_X86_64    = 62;

static constexpr uint32_t PT_LOAD      = 1;
static constexpr uint32_t PT_DYNAMIC   = 2;
static constexpr uint32_t PT_SCE_DYNLIBDATA = 0x61000000;
static constexpr uint32_t PT_SCE_PROC_PARAM = 0x61000001;

static constexpr uint32_t PF_X = 0x1;
static constexpr uint32_t PF_W = 0x2;
static constexpr uint32_t PF_R = 0x4;

// ELF64 header
#pragma pack(push, 1)
struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf64_Dyn {
    int64_t  d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};
#pragma pack(pop)

template <typename T>
static const T* ReadAt(const uint8_t* data, size_t data_size, uint64_t offset) {
    if (offset + sizeof(T) > data_size) return nullptr;
    T tmp;
    std::memcpy(&tmp, data + offset, sizeof(T));
    // Return pointer into data buffer directly (caller must not outlive data)
    return reinterpret_cast<const T*>(data + offset);
}

} // anonymous namespace

namespace Loader {

// ─── Subsystem NID / library keyword table ───────────────────────────────────

namespace {

struct SubsystemKeyword {
    const char*    keyword;
    OrbisSubsystem flag;
};

static const SubsystemKeyword kSubsystemKeywords[] = {
    // Graphics
    { "libSceGnmDriver",         OrbisSubsystem::Graphics   },
    { "libSceVideoOut",          OrbisSubsystem::Graphics   },
    { "libSceGpuTrace",          OrbisSubsystem::Graphics   },
    { "libSceGfxTrace",          OrbisSubsystem::Graphics   },
    // Filesystem
    { "libSceLibcInternal",      OrbisSubsystem::Filesystem },
    { "libSceKernel",            OrbisSubsystem::Filesystem },
    { "libSceFscc",              OrbisSubsystem::Filesystem },
    // Threads
    { "libScePthread",           OrbisSubsystem::Threads    },
    { "libSceNpCommon",          OrbisSubsystem::Threads    },
    // Input
    { "libSceHid",               OrbisSubsystem::Input      },
    { "libScePad",               OrbisSubsystem::Input      },
    { "libSceMove",              OrbisSubsystem::Input      },
    // Audio
    { "libSceAudioOut",          OrbisSubsystem::Audio      },
    { "libSceAudio3d",           OrbisSubsystem::Audio      },
    { "libSceBeq",               OrbisSubsystem::Audio      },
    // Networking
    { "libSceNet",               OrbisSubsystem::Networking },
    { "libSceNetCtl",            OrbisSubsystem::Networking },
    { "libSceHttp",              OrbisSubsystem::Networking },
};

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

OpenOrbisLoadResult OpenOrbisElfLoader::Load(const std::filesystem::path& elf_path,
                                             SymbolResolverFn resolver) {
    OpenOrbisLoadResult result;

    if (!std::filesystem::exists(elf_path)) {
        result.error_message = "ELF file not found: " + elf_path.string();
        return result;
    }

    std::ifstream file(elf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error_message = "Failed to open: " + elf_path.string();
        return result;
    }

    const auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> data(file_size);
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size))) {
        result.error_message = "Failed to read ELF data";
        return result;
    }

    result = LoadFromMemory(data.data(), data.size(), elf_path.filename().string(), resolver);
    m_last_result = result;
    return result;
}

OpenOrbisLoadResult OpenOrbisElfLoader::LoadFromMemory(const uint8_t* data, size_t size,
                                                      const std::string& label,
                                                      SymbolResolverFn resolver) {
    OpenOrbisLoadResult result;

    if (!ParseElfHeader(data, size, result)) {
        return result;
    }
    if (!MapLoadSegments(data, size, result)) {
        return result;
    }
    if (!ParseDynamicSection(data, size, result)) {
        // Non-fatal; static ELFs may have no .dynamic
    }

    ProcessRelocations(data, size, result, resolver);

    DetectSubsystems(result);
    result.module_name = label;
    result.success     = true;

    LOGF("[OpenOrbisElfLoader] Loaded '%s': entry=0x%llx, base=0x%llx, size=0x%llx (relocs=%u, resolved=%u)\n",
                  label.c_str(),
                  (unsigned long long)result.entry_vaddr,
                  (unsigned long long)result.base_vaddr,
                  (unsigned long long)result.image_size,
                  result.reloc_count,
                  result.resolved_symbols_count);

    m_last_result = result;
    return result;
}

bool OpenOrbisElfLoader::ProcessRelocations(const uint8_t* data, size_t size,
                                           OpenOrbisLoadResult& out,
                                           SymbolResolverFn resolver) {
    if (!data || size < sizeof(Elf64_Ehdr)) return false;

    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(data);
    uint64_t dyn_offset = 0;
    uint64_t dyn_size   = 0;

    for (uint16_t i = 0; i < hdr->e_phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf64_Phdr*>(
            data + hdr->e_phoff + i * sizeof(Elf64_Phdr));
        if (ph->p_type == PT_DYNAMIC) {
            dyn_offset = ph->p_offset;
            dyn_size   = ph->p_filesz;
            break;
        }
    }

    if (dyn_offset == 0 || dyn_size == 0 || dyn_offset + dyn_size > size) {
        return false;
    }

    uint64_t rela_vaddr   = 0;
    uint64_t rela_size    = 0;
    uint64_t jmprel_vaddr = 0;
    uint64_t pltrel_size  = 0;
    uint64_t symtab_vaddr = 0;
    uint64_t syment_size  = sizeof(Elf64_Sym);
    uint64_t strtab_vaddr = 0;
    uint64_t strtab_size  = 0;

    const size_t entry_count = dyn_size / sizeof(Elf64_Dyn);
    for (size_t i = 0; i < entry_count; ++i) {
        Elf64_Dyn dyn;
        std::memcpy(&dyn, data + dyn_offset + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));
        if (dyn.d_tag == 0x07 /* DT_RELA */ || dyn.d_tag == DT_SCE_RELA) rela_vaddr = dyn.d_un.d_val;
        if (dyn.d_tag == 0x08 /* DT_RELASZ */ || dyn.d_tag == DT_SCE_RELASZ) rela_size = dyn.d_un.d_val;
        if (dyn.d_tag == 0x17 /* DT_JMPREL */ || dyn.d_tag == DT_SCE_JMPREL) jmprel_vaddr = dyn.d_un.d_val;
        if (dyn.d_tag == 0x02 /* DT_PLTRELSZ */ || dyn.d_tag == DT_SCE_PLTRELSZ) pltrel_size = dyn.d_un.d_val;
        if (dyn.d_tag == 0x06 /* DT_SYMTAB */ || dyn.d_tag == DT_SCE_SYMTAB) symtab_vaddr = dyn.d_un.d_val;
        if (dyn.d_tag == 0x0B /* DT_SYMENT */ || dyn.d_tag == DT_SCE_SYMENT) syment_size = dyn.d_un.d_val;
        if (dyn.d_tag == 0x05 /* DT_STRTAB */ || dyn.d_tag == DT_SCE_STRTAB) strtab_vaddr = dyn.d_un.d_val;
        if (dyn.d_tag == 0x0A /* DT_STRSZ */ || dyn.d_tag == DT_SCE_STRSZ) strtab_size = dyn.d_un.d_val;
    }

    auto vaddr_to_file_offset = [&](uint64_t vaddr) -> uint64_t {
        for (const auto& seg : out.segments) {
            if (vaddr >= seg.guest_vaddr && vaddr < seg.guest_vaddr + seg.file_size) {
                return vaddr - seg.guest_vaddr + seg.host_offset;
            }
        }
        return UINT64_MAX;
    };

    auto get_string = [&](uint64_t str_idx) -> std::string {
        if (strtab_vaddr == 0) return {};
        uint64_t str_target = strtab_vaddr + str_idx;
        uint64_t file_off = vaddr_to_file_offset(str_target);
        if (file_off != UINT64_MAX && file_off < size) {
            return std::string(reinterpret_cast<const char*>(data + file_off));
        }
        if (strtab_vaddr + str_idx < size) {
            return std::string(reinterpret_cast<const char*>(data + strtab_vaddr + str_idx));
        }
        return {};
    };

    auto get_symbol = [&](uint32_t sym_idx, Elf64_Sym& out_sym, std::string& out_name) -> bool {
        if (symtab_vaddr == 0 || sym_idx == 0) return false;
        const uint64_t entry_sz = (syment_size > 0) ? syment_size : sizeof(Elf64_Sym);
        uint64_t sym_target = symtab_vaddr + static_cast<uint64_t>(sym_idx) * entry_sz;
        uint64_t file_off = vaddr_to_file_offset(sym_target);
        if (file_off != UINT64_MAX && file_off + sizeof(Elf64_Sym) <= size) {
            std::memcpy(&out_sym, data + file_off, sizeof(Elf64_Sym));
            out_name = get_string(out_sym.st_name);
            return true;
        }
        if (sym_target + sizeof(Elf64_Sym) <= size) {
            std::memcpy(&out_sym, data + sym_target, sizeof(Elf64_Sym));
            out_name = get_string(out_sym.st_name);
            return true;
        }
        return false;
    };

    // Extract all dynamic symbols into out.symbols if symbol table is present
    if (symtab_vaddr != 0 && strtab_vaddr != 0) {
        uint64_t sym_file_off = vaddr_to_file_offset(symtab_vaddr);
        if (sym_file_off == UINT64_MAX && symtab_vaddr < size) {
            sym_file_off = symtab_vaddr;
        }
        if (sym_file_off != UINT64_MAX) {
            const uint64_t entry_sz = (syment_size > 0) ? syment_size : sizeof(Elf64_Sym);
            size_t max_symbols = 1024;
            for (size_t s = 0; s < max_symbols; ++s) {
                uint64_t cur_off = sym_file_off + s * entry_sz;
                if (cur_off + sizeof(Elf64_Sym) > size) break;
                Elf64_Sym sym;
                std::memcpy(&sym, data + cur_off, sizeof(Elf64_Sym));
                if (sym.st_name == 0 && sym.st_value == 0 && sym.st_info == 0 && s > 0) {
                    break;
                }
                DynamicSymbolInfo dsym;
                dsym.name = get_string(sym.st_name);
                dsym.value = sym.st_value;
                dsym.size = sym.st_size;
                dsym.shndx = sym.st_shndx;
                dsym.type = sym.st_info & 0x0F;
                dsym.bind = sym.st_info >> 4;
                dsym.is_undefined = (sym.st_shndx == 0);
                out.symbols.push_back(std::move(dsym));
            }
        }
    }

    auto patch_reloc_entry = [&](const Elf64_Rela& rela) {
        uint32_t type = static_cast<uint32_t>(rela.r_info & 0xFFFFFFFF);
        uint32_t sym_idx = static_cast<uint32_t>(rela.r_info >> 32);
        uint64_t target_vaddr = rela.r_offset;

        if (target_vaddr >= out.base_vaddr && target_vaddr < out.base_vaddr + out.image_size) {
            uint64_t buf_off = target_vaddr - out.base_vaddr;
            if (buf_off + sizeof(uint64_t) <= out.image_buffer.size()) {
                out.reloc_count++;

                if (type == 8 /* R_X86_64_RELATIVE */) {
                    uint64_t val = out.base_vaddr + rela.r_addend;
                    std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                    out.resolved_symbols_count++;
                } else if (type == 1 /* R_X86_64_64 */ || type == 6 /* R_X86_64_GLOB_DAT */ ||
                           type == 7 /* R_X86_64_JUMP_SLOT */) {
                    Elf64_Sym sym{};
                    std::string sym_name;
                    bool has_sym = get_symbol(sym_idx, sym, sym_name);
                    uint64_t sym_val = 0;
                    bool resolved = false;

                    if (has_sym) {
                        if (sym.st_shndx != 0 /* SHN_UNDEF */) {
                            // Defined in this module
                            sym_val = (out.base_vaddr + sym.st_value);
                            resolved = true;
                        } else if (resolver && !sym_name.empty()) {
                            // External symbol resolution
                            std::string lib_name = !out.import_libs.empty() ? out.import_libs[0] : "";
                            uint64_t resolved_addr = resolver(sym_name, lib_name);
                            if (resolved_addr != 0) {
                                sym_val = resolved_addr;
                                resolved = true;
                            }
                        }
                    }

                    if (resolved) {
                        uint64_t val = sym_val + rela.r_addend;
                        std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                        out.resolved_symbols_count++;
                    } else {
                        if (rela.r_addend != 0) {
                            uint64_t val = out.base_vaddr + rela.r_addend;
                            std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                            out.resolved_symbols_count++;
                        } else {
                            if (!sym_name.empty()) {
                                out.unresolved_symbols.push_back(sym_name);
                            }
                        }
                    }
                } else if (type == 16 /* R_X86_64_DTPMOD64 */) {
                    uint64_t val = 1; // Primary module TLS index
                    std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                    out.resolved_symbols_count++;
                } else if (type == 17 /* R_X86_64_DTPOFF64 */) {
                    Elf64_Sym sym{};
                    std::string sym_name;
                    get_symbol(sym_idx, sym, sym_name);
                    uint64_t val = sym.st_value + rela.r_addend;
                    std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                    out.resolved_symbols_count++;
                } else if (type == 18 /* R_X86_64_TPOFF64 */) {
                    // TLS offset
                    Elf64_Sym sym{};
                    std::string sym_name;
                    get_symbol(sym_idx, sym, sym_name);
                    uint64_t val = sym.st_value + rela.r_addend;
                    std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                    out.resolved_symbols_count++;
                } else if (type == 2 /* R_X86_64_PC32 */ || type == 4 /* R_X86_64_PLT32 */) {
                    Elf64_Sym sym{};
                    std::string sym_name;
                    bool has_sym = get_symbol(sym_idx, sym, sym_name);
                    uint64_t sym_val = has_sym ? (out.base_vaddr + sym.st_value) : 0;
                    int32_t val = static_cast<int32_t>(sym_val + rela.r_addend - (out.base_vaddr + rela.r_offset));
                    std::memcpy(out.image_buffer.data() + buf_off, &val, sizeof(val));
                    out.resolved_symbols_count++;
                }
            }
        }
    };

    auto read_rela_table = [&](uint64_t table_vaddr, uint64_t table_size) {
        if (table_vaddr == 0 || table_size == 0) return;
        uint64_t file_off = vaddr_to_file_offset(table_vaddr);
        if (file_off == UINT64_MAX && table_vaddr < size) {
            file_off = table_vaddr;
        }
        if (file_off != UINT64_MAX) {
            size_t num_relas = table_size / sizeof(Elf64_Rela);
            for (size_t r = 0; r < num_relas; ++r) {
                if (file_off + (r + 1) * sizeof(Elf64_Rela) <= size) {
                    Elf64_Rela rela;
                    std::memcpy(&rela, data + file_off + r * sizeof(Elf64_Rela), sizeof(Elf64_Rela));
                    patch_reloc_entry(rela);
                }
            }
        }
    };

    read_rela_table(rela_vaddr, rela_size);
    read_rela_table(jmprel_vaddr, pltrel_size);

    return true;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

bool OpenOrbisElfLoader::ParseElfHeader(const uint8_t* data, size_t size,
                                         OpenOrbisLoadResult& out) {
    if (size < sizeof(Elf64_Ehdr)) {
        out.error_message = "File too small for ELF header";
        return false;
    }

    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    // Magic
    if (hdr->e_ident[0] != ELFMAG0 || hdr->e_ident[1] != ELFMAG1 ||
        hdr->e_ident[2] != ELFMAG2 || hdr->e_ident[3] != ELFMAG3) {
        out.error_message = "Not an ELF file";
        return false;
    }
    if (hdr->e_ident[4] != ELFCLASS64) {
        out.error_message = "Only ELF64 supported";
        return false;
    }
    if (hdr->e_ident[5] != ELFDATA2LSB) {
        out.error_message = "Only little-endian ELFs supported";
        return false;
    }
    if (hdr->e_machine != EM_X86_64) {
        out.error_message = "ELF machine is not x86-64";
        return false;
    }
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
        out.error_message = "Only ET_EXEC and ET_DYN supported";
        return false;
    }

    out.entry_vaddr = hdr->e_entry;
    return true;
}

bool OpenOrbisElfLoader::MapLoadSegments(const uint8_t* data, size_t size,
                                          OpenOrbisLoadResult& out) {
    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    if (hdr->e_phoff == 0 || hdr->e_phnum == 0) {
        out.error_message = "No program headers";
        return false;
    }
    if (hdr->e_phoff + static_cast<uint64_t>(hdr->e_phnum) * sizeof(Elf64_Phdr) > size) {
        out.error_message = "Program header table out of bounds";
        return false;
    }

    // First pass: determine total guest VA range
    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;

    for (uint16_t i = 0; i < hdr->e_phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf64_Phdr*>(
            data + hdr->e_phoff + i * sizeof(Elf64_Phdr));
        if (ph->p_type != PT_LOAD && ph->p_type != PT_SCE_DYNLIBDATA) continue;
        if (ph->p_memsz == 0) continue;
        min_vaddr = std::min(min_vaddr, ph->p_vaddr);
        max_vaddr = std::max(max_vaddr, ph->p_vaddr + ph->p_memsz);
    }

    if (min_vaddr == UINT64_MAX) {
        out.error_message = "No loadable segments";
        return false;
    }

    const uint64_t image_size = max_vaddr - min_vaddr;
    out.base_vaddr  = min_vaddr;
    out.image_size  = image_size;

    // Allocate flat guest image buffer (host backing store)
    out.image_buffer.assign(image_size, 0);

    // Second pass: copy segment data
    for (uint16_t i = 0; i < hdr->e_phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf64_Phdr*>(
            data + hdr->e_phoff + i * sizeof(Elf64_Phdr));

        if (ph->p_type == PT_SCE_PROC_PARAM) {
            out.proc_param_vaddr = ph->p_vaddr;
        }

        if (ph->p_type != PT_LOAD && ph->p_type != PT_SCE_DYNLIBDATA) continue;
        if (ph->p_memsz == 0) continue;

        if (ph->p_offset + ph->p_filesz > size) {
            out.error_message = "Segment file data out of bounds";
            return false;
        }

        const uint64_t seg_offset = ph->p_vaddr - min_vaddr;
        if (seg_offset + ph->p_memsz > image_size) {
            out.error_message = "Segment exceeds allocated image buffer";
            return false;
        }

        if (ph->p_filesz > 0) {
            std::memcpy(out.image_buffer.data() + seg_offset,
                        data + ph->p_offset,
                        ph->p_filesz);
        }

        OrbisLoadedSegment seg;
        seg.guest_vaddr = ph->p_vaddr;
        seg.host_offset = seg_offset;
        seg.file_size   = ph->p_filesz;
        seg.mem_size    = ph->p_memsz;
        seg.flags       = ph->p_flags;
        out.segments.push_back(seg);
    }

    return true;
}

bool OpenOrbisElfLoader::ParseDynamicSection(const uint8_t* data, size_t size,
                                              OpenOrbisLoadResult& out) {
    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    // Find PT_DYNAMIC segment
    uint64_t dyn_offset = 0;
    uint64_t dyn_size   = 0;
    uint64_t strtab_offset = 0;
    uint64_t strtab_size   = 0;

    for (uint16_t i = 0; i < hdr->e_phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf64_Phdr*>(
            data + hdr->e_phoff + i * sizeof(Elf64_Phdr));
        if (ph->p_type == PT_DYNAMIC) {
            dyn_offset = ph->p_offset;
            dyn_size   = ph->p_filesz;
            break;
        }
    }

    if (dyn_offset == 0 || dyn_size == 0) {
        return false; // No .dynamic — static ELF, that's OK
    }

    if (dyn_offset + dyn_size > size) return false;

    // First pass: find SCE string table
    const size_t entry_count = dyn_size / sizeof(Elf64_Dyn);
    for (size_t i = 0; i < entry_count; ++i) {
        Elf64_Dyn dyn;
        std::memcpy(&dyn, data + dyn_offset + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));
        if (dyn.d_tag == DT_SCE_STRTAB)  strtab_offset = dyn.d_un.d_val;
        if (dyn.d_tag == DT_SCE_STRSZ)   strtab_size   = dyn.d_un.d_val;
        if (dyn.d_tag == 0x05 /* DT_STRTAB */ && strtab_offset == 0)
            strtab_offset = dyn.d_un.d_val;
        if (dyn.d_tag == 0x0A /* DT_STRSZ */ && strtab_size == 0)
            strtab_size = dyn.d_un.d_val;
    }

    auto read_string = [&](uint64_t str_offset) -> std::string {
        // str_offset is a VA — convert to file offset via segment mapping
        for (const auto& seg : out.segments) {
            if (str_offset >= seg.guest_vaddr && str_offset < seg.guest_vaddr + seg.file_size) {
                const uint64_t off = str_offset - seg.guest_vaddr + seg.host_offset;
                if (off < out.image_buffer.size()) {
                    return std::string(reinterpret_cast<const char*>(
                        out.image_buffer.data() + off));
                }
            }
        }
        // Fallback: try strtab_offset as a file offset
        if (strtab_offset + str_offset < size) {
            return std::string(reinterpret_cast<const char*>(
                data + strtab_offset + str_offset));
        }
        return {};
    };

    // Second pass: extract module / library names
    for (size_t i = 0; i < entry_count; ++i) {
        Elf64_Dyn dyn;
        std::memcpy(&dyn, data + dyn_offset + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));

        if (dyn.d_tag == DT_SCE_MODULE_INFO) {
            out.module_name = read_string(dyn.d_un.d_val & 0xFFFFFFFF);
        } else if (dyn.d_tag == DT_SCE_NEEDED_MODULE) {
            auto name = read_string(dyn.d_un.d_val & 0xFFFFFFFF);
            if (!name.empty()) out.needed_modules.push_back(std::move(name));
        } else if (dyn.d_tag == DT_SCE_IMPORT_LIB) {
            auto name = read_string(dyn.d_un.d_val & 0xFFFFFFFF);
            if (!name.empty()) out.import_libs.push_back(std::move(name));
        } else if (dyn.d_tag == DT_SCE_EXPORT_LIB) {
            auto name = read_string(dyn.d_un.d_val & 0xFFFFFFFF);
            if (!name.empty()) out.export_libs.push_back(std::move(name));
        } else if (dyn.d_tag == DT_SCE_PROC_PARAM) {
            out.proc_param_vaddr = dyn.d_un.d_val;
        } else if (dyn.d_tag == 0x01 /* DT_NEEDED */) {
            auto name = read_string(dyn.d_un.d_val);
            if (!name.empty()) out.needed_modules.push_back(std::move(name));
        }
    }

    return true;
}

void OpenOrbisElfLoader::DetectSubsystems(OpenOrbisLoadResult& out) {
    auto check = [&](const std::string& name) {
        for (const auto& kw : kSubsystemKeywords) {
            if (name.find(kw.keyword) != std::string::npos) {
                out.detected_subsystems = out.detected_subsystems | kw.flag;
            }
        }
    };

    for (const auto& m : out.needed_modules) check(m);
    for (const auto& l : out.import_libs)    check(l);

    // Homebrew Hello World executables often have no .dynamic; give them FS by default
    if (out.needed_modules.empty() && out.import_libs.empty()) {
        out.detected_subsystems = out.detected_subsystems | OrbisSubsystem::Filesystem;
    }
}

} // namespace Loader
