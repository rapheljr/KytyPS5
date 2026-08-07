// openOrbisSubsystems.cpp
//
// OpenOrbis / PS5 Homebrew Subsystem Stub Hub Implementation.
//
// All stubs:
//   1. Log the call (via LOGF at trace level).
//   2. Record it in the JIT telemetry collector.
//   3. Return a plausible success code so homebrew can continue running.

#include "kernel/openOrbisSubsystems.h"

#include "common/logging/log.h"
#include "graphics/presentation/videoOut.h"
#include "libs/agc.h"

#include <cstring>

namespace Kernel {

OpenOrbisSubsystemHub::OpenOrbisSubsystemHub(
    Loader::Recompiler::JitTelemetryCollector& telemetry)
    : m_telemetry(telemetry) {}

void OpenOrbisSubsystemHub::RegisterAll() {
    // ── Filesystem ────────────────────────────────────────────────────────────
    RegisterStub("sceKernelOpen",
        [this](const SubsystemCallCtx& ctx) { return StubKernelOpen(ctx); });
    RegisterStub("sceKernelRead",
        [this](const SubsystemCallCtx& ctx) { return StubKernelRead(ctx); });
    RegisterStub("sceKernelWrite",
        [this](const SubsystemCallCtx& ctx) { return StubKernelWrite(ctx); });
    RegisterStub("sceKernelClose",
        [this](const SubsystemCallCtx& ctx) { return StubKernelClose(ctx); });
    RegisterStub("sceKernelLseek",
        [this](const SubsystemCallCtx& ctx) { return StubKernelLseek(ctx); });
    RegisterStub("sceKernelStat",
        [this](const SubsystemCallCtx& ctx) { return StubKernelStat(ctx); });
    RegisterStub("sceKernelMmap",
        [this](const SubsystemCallCtx& ctx) { return StubKernelMmap(ctx); });
    RegisterStub("sceKernelMunmap",
        [this](const SubsystemCallCtx& ctx) { return StubKernelMunmap(ctx); });

    // ── Threads ───────────────────────────────────────────────────────────────
    RegisterStub("scePthreadCreate",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadCreate(ctx); });
    RegisterStub("scePthreadJoin",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadJoin(ctx); });
    RegisterStub("scePthreadExit",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadExit(ctx); });
    RegisterStub("scePthreadMutexLock",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadMutexLock(ctx); });
    RegisterStub("scePthreadMutexUnlock",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadMutexUnlock(ctx); });
    RegisterStub("scePthreadMutexInit",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadMutexInit(ctx); });
    RegisterStub("scePthreadMutexDestroy",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadMutexDestroy(ctx); });
    RegisterStub("scePthreadCondWait",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadCondWait(ctx); });
    RegisterStub("scePthreadCondSignal",
        [this](const SubsystemCallCtx& ctx) { return StubPthreadCondSignal(ctx); });

    // ── Input ─────────────────────────────────────────────────────────────────
    RegisterStub("sceHidServiceOpen",
        [this](const SubsystemCallCtx& ctx) { return StubHidServiceOpen(ctx); });
    RegisterStub("sceHidServiceGetControllerState",
        [this](const SubsystemCallCtx& ctx) { return StubHidServiceGetControllerState(ctx); });
    RegisterStub("sceHidServiceClose",
        [this](const SubsystemCallCtx& ctx) { return StubHidServiceClose(ctx); });
    RegisterStub("scePadOpen",
        [this](const SubsystemCallCtx& ctx) { return StubPadOpen(ctx); });
    RegisterStub("scePadReadState",
        [this](const SubsystemCallCtx& ctx) { return StubPadReadState(ctx); });
    RegisterStub("scePadClose",
        [this](const SubsystemCallCtx& ctx) { return StubPadClose(ctx); });

    // ── Audio ─────────────────────────────────────────────────────────────────
    RegisterStub("sceAudioOutOpen",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutOpen(ctx); });
    RegisterStub("sceAudioOutOutput",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutOutput(ctx); });
    RegisterStub("sceAudioOutClose",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutClose(ctx); });
    RegisterStub("sceAudioOutGetLastOutputTimestamp",
        [this](const SubsystemCallCtx& ctx) {
            return StubAudioOutGetLastOutputTimestamp(ctx); });

    // ── Networking ────────────────────────────────────────────────────────────
    RegisterStub("sceNetSocket",
        [this](const SubsystemCallCtx& ctx) { return StubNetSocket(ctx); });
    RegisterStub("sceNetConnect",
        [this](const SubsystemCallCtx& ctx) { return StubNetConnect(ctx); });
    RegisterStub("sceNetSend",
        [this](const SubsystemCallCtx& ctx) { return StubNetSend(ctx); });
    RegisterStub("sceNetRecv",
        [this](const SubsystemCallCtx& ctx) { return StubNetRecv(ctx); });
    RegisterStub("sceNetClose",
        [this](const SubsystemCallCtx& ctx) { return StubNetClose(ctx); });
    RegisterStub("sceNetBind",
        [this](const SubsystemCallCtx& ctx) { return StubNetBind(ctx); });
    RegisterStub("sceNetListen",
        [this](const SubsystemCallCtx& ctx) { return StubNetListen(ctx); });
    RegisterStub("sceNetAccept",
        [this](const SubsystemCallCtx& ctx) { return StubNetAccept(ctx); });

    // ── Graphics ──────────────────────────────────────────────────────────────
    RegisterStub("sceVideoOutOpen",
        [this](const SubsystemCallCtx& ctx) { return StubVideoOutOpen(ctx); });
    RegisterStub("sceVideoOutSetFlipRate",
        [this](const SubsystemCallCtx& ctx) { return StubVideoOutSetFlipRate(ctx); });
    RegisterStub("sceVideoOutSubmitFlip",
        [this](const SubsystemCallCtx& ctx) { return StubVideoOutSubmitFlip(ctx); });
    RegisterStub("sceVideoOutClose",
        [this](const SubsystemCallCtx& ctx) { return StubVideoOutClose(ctx); });
    RegisterStub("sceGnmSubmitCommandBuffers",
        [this](const SubsystemCallCtx& ctx) { return StubGnmSubmitCommandBuffers(ctx); });
    RegisterStub("sceGnmFlushGarlic",
        [this](const SubsystemCallCtx& ctx) { return StubGnmFlushGarlic(ctx); });
}

void OpenOrbisSubsystemHub::RegisterStub(std::string name, SubsystemStubFn fn) {
    m_stubs[std::move(name)] = std::move(fn);
}

const SubsystemStubFn* OpenOrbisSubsystemHub::FindStub(const std::string& name) const {
    auto it = m_stubs.find(name);
    return (it != m_stubs.end()) ? &it->second : nullptr;
}

int64_t OpenOrbisSubsystemHub::Dispatch(const std::string& name,
                                         const SubsystemCallCtx& ctx) {
    const auto* fn = FindStub(name);
    if (!fn) {
        LOGF("[SubsystemHub] Unregistered stub: %s\n", name.c_str());
        m_telemetry.RecordSyscall("UNIMPL:" + name);
        return SCE_ERROR_ERRNO_ENOSYS;
    }
    m_telemetry.RecordSyscall(name);
    return (*fn)(ctx);
}

// ─── Filesystem stubs ─────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubKernelOpen(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceKernelOpen -> fd=%d\n", m_next_fd);
    return m_next_fd++;
}
int64_t OpenOrbisSubsystemHub::StubKernelRead(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelRead(fd=%llu, nbytes=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg2);
    return static_cast<int64_t>(ctx.arg2); // Claim full read
}
int64_t OpenOrbisSubsystemHub::StubKernelWrite(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelWrite(fd=%llu, nbytes=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg2);
    return static_cast<int64_t>(ctx.arg2);
}
int64_t OpenOrbisSubsystemHub::StubKernelClose(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelClose(fd=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubKernelLseek(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelLseek(fd=%llu, offset=%llu, whence=%llu)\n",
         (unsigned long long)ctx.arg0,
         (unsigned long long)ctx.arg1,
         (unsigned long long)ctx.arg2);
    return static_cast<int64_t>(ctx.arg1);
}
int64_t OpenOrbisSubsystemHub::StubKernelStat(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelStat(path=0x%llx)\n", (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg1), 0, 144);
    }
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubKernelMmap(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceKernelMmap(len=0x%llx)\n", (unsigned long long)ctx.arg1);
    return static_cast<int64_t>(0x20000000 + ctx.arg1);
}
int64_t OpenOrbisSubsystemHub::StubKernelMunmap(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceKernelMunmap\n");
    return SCE_OK;
}

// ─── Thread stubs ─────────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubPthreadCreate(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadCreate\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadJoin(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePthreadJoin(tid=0x%llx)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadExit(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePthreadExit(code=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadMutexLock(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadMutexLock\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadMutexUnlock(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadMutexUnlock\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadMutexInit(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadMutexInit\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadMutexDestroy(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadMutexDestroy\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadCondWait(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadCondWait\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPthreadCondSignal(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] scePthreadCondSignal\n");
    return SCE_OK;
}

// ─── Input stubs ──────────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubHidServiceOpen(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceHidServiceOpen -> handle=0\n");
    return 0;
}
int64_t OpenOrbisSubsystemHub::StubHidServiceGetControllerState(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceHidServiceGetControllerState(handle=%llu)\n",
         (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg1), 0, 80);
    }
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubHidServiceClose(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceHidServiceClose\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadOpen(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadOpen(userId=%llu, type=%llu) -> handle=1\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return 1;
}
int64_t OpenOrbisSubsystemHub::StubPadReadState(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadReadState(handle=%llu)\n", (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg1), 0, 96);
    }
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadClose(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadClose(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}

// ─── Audio stubs ──────────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubAudioOutOpen(const SubsystemCallCtx& ctx) {
    const int32_t handle = m_next_audio++;
    LOGF("[SubsystemHub] sceAudioOutOpen(type=%llu) -> handle=%d\n",
         (unsigned long long)ctx.arg0, handle);
    return handle;
}
int64_t OpenOrbisSubsystemHub::StubAudioOutOutput(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceAudioOutOutput(handle=%llu, ptr=0x%llx)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubAudioOutClose(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceAudioOutClose(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubAudioOutGetLastOutputTimestamp(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceAudioOutGetLastOutputTimestamp(handle=%llu)\n",
         (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        uint64_t ts = 0;
        std::memcpy(reinterpret_cast<void*>(ctx.arg1), &ts, sizeof(ts));
    }
    return SCE_OK;
}

// ─── Networking stubs ─────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubNetSocket(const SubsystemCallCtx& ctx) {
    const int32_t sock = m_next_sock++;
    LOGF("[SubsystemHub] sceNetSocket(domain=%llu, type=%llu, proto=%llu) -> sock=%d\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1,
         (unsigned long long)ctx.arg2, sock);
    return sock;
}
int64_t OpenOrbisSubsystemHub::StubNetConnect(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetConnect(sock=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubNetSend(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetSend(sock=%llu, len=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg2);
    return static_cast<int64_t>(ctx.arg2);
}
int64_t OpenOrbisSubsystemHub::StubNetRecv(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetRecv(sock=%llu, len=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg2);
    return 0; // EOF — graceful close
}
int64_t OpenOrbisSubsystemHub::StubNetClose(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetClose(sock=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubNetBind(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetBind(sock=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubNetListen(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetListen(sock=%llu, backlog=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubNetAccept(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNetAccept(sock=%llu)\n", (unsigned long long)ctx.arg0);
    return m_next_sock++;
}

// ─── Graphics stubs ───────────────────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubVideoOutOpen(const SubsystemCallCtx& ctx) {
    const int32_t handle = m_next_video++;
    LOGF("[SubsystemHub] sceVideoOutOpen(userId=%llu) -> handle=%d\n",
         (unsigned long long)ctx.arg0, handle);
    return handle;
}
int64_t OpenOrbisSubsystemHub::StubVideoOutSetFlipRate(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceVideoOutSetFlipRate(handle=%llu, rate=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubVideoOutSubmitFlip(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceVideoOutSubmitFlip(handle=%llu, fb_idx=%llu)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    m_telemetry.RecordPm4Packet();
    return Libs::VideoOut::VideoOutSubmitFlip(
        static_cast<int>(ctx.arg0), static_cast<int>(ctx.arg1), 0, 0);
}
int64_t OpenOrbisSubsystemHub::StubVideoOutClose(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceVideoOutClose(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubGnmSubmitCommandBuffers(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceGnmSubmitCommandBuffers(numCBs=%llu, dcbGpuAddrs=0x%llx, dcbSizes=0x%llx)\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1, (unsigned long long)ctx.arg2);
    m_telemetry.RecordPm4Packet();
    uint32_t count = static_cast<uint32_t>(ctx.arg0);
    auto* const* dcb_addrs = reinterpret_cast<uint32_t* const*>(ctx.arg1);
    const auto* dcb_sizes = reinterpret_cast<const uint32_t*>(ctx.arg2);
    if (count > 0 && dcb_addrs != nullptr && dcb_sizes != nullptr) {
        Libs::Graphics::Gen5Driver::GraphicsDriverSubmitMultiDcbs(dcb_addrs, dcb_sizes, count);
    }
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubGnmFlushGarlic(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceGnmFlushGarlic\n");
    return SCE_OK;
}

} // namespace Kernel
