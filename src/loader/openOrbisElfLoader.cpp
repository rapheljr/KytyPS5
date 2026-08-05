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
    uint64_t d_val; // or d_ptr
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

OpenOrbisLoadResult OpenOrbisElfLoader::Load(const std::filesystem::path& elf_path) {
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

    result = LoadFromMemory(data.data(), data.size(), elf_path.filename().string());
    m_last_result = result;
    return result;
}

OpenOrbisLoadResult OpenOrbisElfLoader::LoadFromMemory(const uint8_t* data, size_t size,
                                                        const std::string& label) {
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

    DetectSubsystems(result);
    result.module_name = label;
    result.success     = true;

    LOGF("[OpenOrbisElfLoader] Loaded '%s': entry=0x%llx, base=0x%llx, size=0x%llx\n",
                  label.c_str(),
                  (unsigned long long)result.entry_vaddr,
                  (unsigned long long)result.base_vaddr,
                  (unsigned long long)result.image_size);

    m_last_result = result;
    return result;
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
