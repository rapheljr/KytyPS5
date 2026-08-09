// openOrbisElfLoader.h
//
// OpenOrbis / PS5 Homebrew ELF64 Loader.
//
// Parses ET_EXEC / ET_DYN ELF64 binaries produced by the OpenOrbis SDK.
// Maps PT_LOAD segments into guest virtual address space.
// Resolves SCE-specific dynamic tags and homebrew module info.
// Returns an OpenOrbisLoadResult with the entry vaddr and detected subsystem flags.

#ifndef LOADER_OPEN_ORBIS_ELF_LOADER_H
#define LOADER_OPEN_ORBIS_ELF_LOADER_H

#include "common/common.h"
#include "loader/elf.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace Loader {

// ─── Dynamic symbol descriptor ───────────────────────────────────────────────

struct DynamicSymbolInfo {
    std::string name;
    uint64_t    value = 0;
    uint64_t    size  = 0;
    uint16_t    shndx = 0;
    uint8_t     type  = 0;
    uint8_t     bind  = 0;
    bool        is_undefined = false;
};

using SymbolResolverFn = std::function<uint64_t(const std::string& name, const std::string& library)>;

// ─── SCE-specific ELF dynamic tag extensions ────────────────────────────────

static constexpr int64_t DT_SCE_FINGERPRINT       = 0x61000007;
static constexpr int64_t DT_SCE_ORIGINAL_FILENAME  = 0x61000009;
static constexpr int64_t DT_SCE_MODULE_INFO        = 0x6100000d;
static constexpr int64_t DT_SCE_NEEDED_MODULE      = 0x6100000f;
static constexpr int64_t DT_SCE_MODULE_ATTR        = 0x61000011;
static constexpr int64_t DT_SCE_EXPORT_LIB         = 0x61000013;
static constexpr int64_t DT_SCE_IMPORT_LIB         = 0x61000015;
static constexpr int64_t DT_SCE_EXPORT_LIB_ATTR    = 0x61000017;
static constexpr int64_t DT_SCE_IMPORT_LIB_ATTR    = 0x61000019;
static constexpr int64_t DT_SCE_HASH               = 0x61000025;
static constexpr int64_t DT_SCE_PLTGOT             = 0x61000027;
static constexpr int64_t DT_SCE_JMPREL             = 0x61000029;
static constexpr int64_t DT_SCE_PLTREL             = 0x6100002b;
static constexpr int64_t DT_SCE_PLTRELSZ           = 0x6100002d;
static constexpr int64_t DT_SCE_RELA               = 0x6100002f;
static constexpr int64_t DT_SCE_RELASZ             = 0x61000031;
static constexpr int64_t DT_SCE_RELAENT            = 0x61000033;
static constexpr int64_t DT_SCE_STRTAB             = 0x61000035;
static constexpr int64_t DT_SCE_STRSZ              = 0x61000037;
static constexpr int64_t DT_SCE_SYMTAB             = 0x61000039;
static constexpr int64_t DT_SCE_SYMENT             = 0x6100003b;
static constexpr int64_t DT_SCE_PROC_PARAM         = 0x61000081;

// ─── Subsystem detection flags ───────────────────────────────────────────────

enum class OrbisSubsystem : uint32_t {
    None        = 0,
    Graphics    = 1 << 0,   // sceVideoOut / libSceGnmDriver
    Filesystem  = 1 << 1,   // sceKernelOpen / sceKernelRead
    Threads     = 1 << 2,   // scePthread*
    Input       = 1 << 3,   // sceHidService*
    Audio       = 1 << 4,   // sceAudioOut*
    Networking  = 1 << 5,   // sceNet*
};

inline OrbisSubsystem operator|(OrbisSubsystem a, OrbisSubsystem b) {
    return static_cast<OrbisSubsystem>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool HasSubsystem(OrbisSubsystem flags, OrbisSubsystem test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

// ─── Loaded segment descriptor ───────────────────────────────────────────────

struct OrbisLoadedSegment {
    uint64_t guest_vaddr = 0;    // Mapped guest virtual address
    uint64_t host_offset = 0;    // Offset into mapped host buffer
    uint64_t file_size   = 0;    // Bytes from ELF file
    uint64_t mem_size    = 0;    // Bytes in memory (including zero-fill)
    uint32_t flags       = 0;    // PF_R | PF_W | PF_X
};

// ─── Load result ─────────────────────────────────────────────────────────────

struct OpenOrbisLoadResult {
    bool     success                = false;
    uint64_t entry_vaddr            = 0;     // _start / e_entry
    uint64_t proc_param_vaddr       = 0;     // DT_SCE_PROC_PARAM
    uint64_t base_vaddr             = 0;     // Lowest mapped segment address
    uint64_t image_size             = 0;     // Total mapped byte range
    uint32_t reloc_count            = 0;     // Total relocation entries processed
    uint32_t resolved_symbols_count = 0;     // Dynamic PRX symbols resolved

    std::string              title_id;      // From PARAM.SFO if available
    std::string              module_name;   // DT_SCE_MODULE_INFO
    std::vector<std::string> needed_modules;
    std::vector<std::string> import_libs;
    std::vector<std::string> export_libs;

    std::vector<DynamicSymbolInfo> symbols;
    std::vector<std::string>       unresolved_symbols;

    OrbisSubsystem detected_subsystems = OrbisSubsystem::None;

    std::vector<OrbisLoadedSegment> segments;
    std::vector<uint8_t>            image_buffer; // Full guest memory image

    std::string error_message;
};

// ─── Loader class ────────────────────────────────────────────────────────────

class OpenOrbisElfLoader {
public:
    OpenOrbisElfLoader() = default;
    ~OpenOrbisElfLoader() = default;

    KYTY_CLASS_NO_COPY(OpenOrbisElfLoader);

    /// Load a homebrew ELF binary from @p elf_path.
    /// On success result.success == true and result.entry_vaddr is valid.
    [[nodiscard]] OpenOrbisLoadResult Load(const std::filesystem::path& elf_path,
                                           SymbolResolverFn resolver = nullptr);

    /// Load from an in-memory ELF image (for tests / inline stubs).
    [[nodiscard]] OpenOrbisLoadResult LoadFromMemory(const uint8_t* data, size_t size,
                                                     const std::string& label = "memory",
                                                     SymbolResolverFn resolver = nullptr);

    /// Process relocation table entries (R_X86_64_RELATIVE, R_X86_64_JUMP_SLOT, R_X86_64_GLOB_DAT).
    static bool ProcessRelocations(const uint8_t* data, size_t size, OpenOrbisLoadResult& out,
                                   SymbolResolverFn resolver = nullptr);

    /// Return the last loaded result (if any).
    [[nodiscard]] const OpenOrbisLoadResult& GetLastResult() const noexcept {
        return m_last_result;
    }

private:
    bool ParseElfHeader(const uint8_t* data, size_t size, OpenOrbisLoadResult& out);
    bool MapLoadSegments(const uint8_t* data, size_t size, OpenOrbisLoadResult& out);
    bool ParseDynamicSection(const uint8_t* data, size_t size, OpenOrbisLoadResult& out);
    void DetectSubsystems(OpenOrbisLoadResult& out);

    OpenOrbisLoadResult m_last_result;
};

} // namespace Loader

#endif // LOADER_OPEN_ORBIS_ELF_LOADER_H
