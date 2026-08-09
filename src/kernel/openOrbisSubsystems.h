// openOrbisSubsystems.h
//
// OpenOrbis / PS5 Homebrew Subsystem Stub Hub.
//
// Provides syscall stubs for all major subsystems required by OpenOrbis homebrew:
//   Filesystem  — sceKernelOpen / Read / Close / Lseek / Stat
//   Threads     — scePthreadCreate / Join / MutexLock / MutexUnlock
//   Input       — sceHidServiceOpen / GetControllerState
//   Audio       — sceAudioOutOpen / Output / Close
//   Networking  — sceNetSocket / Connect / Send / Recv / Close
//   Graphics    — sceVideoOutOpen / SetFlipRate / SubmitFlip
//
// Each stub logs the call, records it in the telemetry collector, and
// returns a plausible success code so homebrew can continue execution.
//
// Stubs are registered into the PS5 SyscallDispatcher via RegisterNid().

#ifndef KERNEL_OPEN_ORBIS_SUBSYSTEMS_H
#define KERNEL_OPEN_ORBIS_SUBSYSTEMS_H

#include "common/common.h"
#include "loader/openOrbisElfLoader.h"
#include "loader/recompiler/jitTelemetryCollector.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace Kernel {

// ─── Stub return codes (PS4/PS5 errno style) ─────────────────────────────────

static constexpr int32_t SCE_OK                  =  0;
static constexpr int32_t SCE_ERROR_ERRNO_EINVAL   = -0x7FFF1306;
static constexpr int32_t SCE_ERROR_ERRNO_ENOSYS   = -0x7FFF1326;
static constexpr int32_t SCE_ERROR_ERRNO_ENOENT   = -0x7FFF1302;
static constexpr int32_t SCE_ERROR_NET_EINVAL      = -0x7FECE706;

// ─── Subsystem call context passed to each stub ───────────────────────────────

struct SubsystemCallCtx {
    uint64_t arg0 = 0;
    uint64_t arg1 = 0;
    uint64_t arg2 = 0;
    uint64_t arg3 = 0;
    uint64_t arg4 = 0;
    uint64_t arg5 = 0;
};

using SubsystemStubFn = std::function<int64_t(const SubsystemCallCtx&)>;

// ─── Hub class ────────────────────────────────────────────────────────────────

class OpenOrbisSubsystemHub {
public:
    explicit OpenOrbisSubsystemHub(Loader::Recompiler::JitTelemetryCollector& telemetry);
    ~OpenOrbisSubsystemHub() = default;

    KYTY_CLASS_NO_COPY(OpenOrbisSubsystemHub);

    /// Register all built-in stubs (call once at emulator init).
    void RegisterAll();

    /// Look up a registered stub by name. Returns nullptr if not found.
    [[nodiscard]] const SubsystemStubFn* FindStub(const std::string& name) const;

    /// Dispatch a call by stub name. Returns SCE_ERROR_ERRNO_ENOSYS if unregistered.
    int64_t Dispatch(const std::string& name, const SubsystemCallCtx& ctx);

    /// Return a SymbolResolverFn lambda that binds to this hub instance.
    [[nodiscard]] Loader::SymbolResolverFn CreateSymbolResolver(uint64_t stub_base_vaddr = 0x80000000);

    /// Total number of registered stubs.
    [[nodiscard]] size_t StubCount() const noexcept { return m_stubs.size(); }

private:
    void RegisterStub(std::string name, SubsystemStubFn fn);

    // ── Stub implementations ─────────────────────────────────────────────────

    // Filesystem
    int64_t StubKernelOpen(const SubsystemCallCtx& ctx);
    int64_t StubKernelRead(const SubsystemCallCtx& ctx);
    int64_t StubKernelWrite(const SubsystemCallCtx& ctx);
    int64_t StubKernelClose(const SubsystemCallCtx& ctx);
    int64_t StubKernelLseek(const SubsystemCallCtx& ctx);
    int64_t StubKernelStat(const SubsystemCallCtx& ctx);
    int64_t StubKernelMmap(const SubsystemCallCtx& ctx);
    int64_t StubKernelMunmap(const SubsystemCallCtx& ctx);

    // Threads
    int64_t StubPthreadCreate(const SubsystemCallCtx& ctx);
    int64_t StubPthreadJoin(const SubsystemCallCtx& ctx);
    int64_t StubPthreadExit(const SubsystemCallCtx& ctx);
    int64_t StubPthreadMutexLock(const SubsystemCallCtx& ctx);
    int64_t StubPthreadMutexUnlock(const SubsystemCallCtx& ctx);
    int64_t StubPthreadMutexInit(const SubsystemCallCtx& ctx);
    int64_t StubPthreadMutexDestroy(const SubsystemCallCtx& ctx);
    int64_t StubPthreadCondWait(const SubsystemCallCtx& ctx);
    int64_t StubPthreadCondSignal(const SubsystemCallCtx& ctx);

    // Input
    int64_t StubHidServiceOpen(const SubsystemCallCtx& ctx);
    int64_t StubHidServiceGetControllerState(const SubsystemCallCtx& ctx);
    int64_t StubHidServiceClose(const SubsystemCallCtx& ctx);
    int64_t StubPadOpen(const SubsystemCallCtx& ctx);
    int64_t StubPadReadState(const SubsystemCallCtx& ctx);
    int64_t StubPadClose(const SubsystemCallCtx& ctx);

    // Audio
    int64_t StubAudioOutOpen(const SubsystemCallCtx& ctx);
    int64_t StubAudioOutOutput(const SubsystemCallCtx& ctx);
    int64_t StubAudioOutClose(const SubsystemCallCtx& ctx);
    int64_t StubAudioOutGetLastOutputTimestamp(const SubsystemCallCtx& ctx);

    // Networking
    int64_t StubNetSocket(const SubsystemCallCtx& ctx);
    int64_t StubNetConnect(const SubsystemCallCtx& ctx);
    int64_t StubNetSend(const SubsystemCallCtx& ctx);
    int64_t StubNetRecv(const SubsystemCallCtx& ctx);
    int64_t StubNetClose(const SubsystemCallCtx& ctx);
    int64_t StubNetBind(const SubsystemCallCtx& ctx);
    int64_t StubNetListen(const SubsystemCallCtx& ctx);
    int64_t StubNetAccept(const SubsystemCallCtx& ctx);

    // Graphics
    int64_t StubVideoOutOpen(const SubsystemCallCtx& ctx);
    int64_t StubVideoOutSetFlipRate(const SubsystemCallCtx& ctx);
    int64_t StubVideoOutSubmitFlip(const SubsystemCallCtx& ctx);
    int64_t StubVideoOutClose(const SubsystemCallCtx& ctx);
    int64_t StubGnmSubmitCommandBuffers(const SubsystemCallCtx& ctx);
    int64_t StubGnmFlushGarlic(const SubsystemCallCtx& ctx);

    std::unordered_map<std::string, SubsystemStubFn> m_stubs;
    Loader::Recompiler::JitTelemetryCollector&        m_telemetry;
    int32_t m_next_fd      = 10;  // Simulated file descriptor counter
    int32_t m_next_sock    = 100; // Simulated socket counter
    int32_t m_next_audio   = 200; // Simulated audio handle counter
    int32_t m_next_video   = 300; // Simulated video out handle counter
};

} // namespace Kernel

#endif // KERNEL_OPEN_ORBIS_SUBSYSTEMS_H
