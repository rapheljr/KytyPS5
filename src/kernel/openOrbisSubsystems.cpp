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
    RegisterStub("scePadSetActuatorEffect",
        [this](const SubsystemCallCtx& ctx) { return StubPadSetActuatorEffect(ctx); });
    RegisterStub("scePadSetLightBar",
        [this](const SubsystemCallCtx& ctx) { return StubPadSetLightBar(ctx); });
    RegisterStub("scePadSetTriggerEffect",
        [this](const SubsystemCallCtx& ctx) { return StubPadSetTriggerEffect(ctx); });
    RegisterStub("scePadGetHandle",
        [this](const SubsystemCallCtx& ctx) { return StubPadGetHandle(ctx); });
    RegisterStub("scePadGetMotionData",
        [this](const SubsystemCallCtx& ctx) { return StubPadGetMotionData(ctx); });
    RegisterStub("scePadGetTouchData",
        [this](const SubsystemCallCtx& ctx) { return StubPadGetTouchData(ctx); });

    // ── Common Dialogs & IME ──────────────────────────────────────────────────
    RegisterStub("sceCommonDialogInitialize",
        [this](const SubsystemCallCtx& ctx) { return StubCommonDialogInitialize(ctx); });
    RegisterStub("sceMsgDialogOpen",
        [this](const SubsystemCallCtx& ctx) { return StubMsgDialogOpen(ctx); });
    RegisterStub("sceMsgDialogGetStatus",
        [this](const SubsystemCallCtx& ctx) { return StubMsgDialogGetStatus(ctx); });
    RegisterStub("sceMsgDialogClose",
        [this](const SubsystemCallCtx& ctx) { return StubMsgDialogClose(ctx); });
    RegisterStub("sceImeDialogOpen",
        [this](const SubsystemCallCtx& ctx) { return StubImeDialogOpen(ctx); });
    RegisterStub("sceImeDialogGetResult",
        [this](const SubsystemCallCtx& ctx) { return StubImeDialogGetResult(ctx); });

    // ── Audio & Voice ────────────────────────────────────────────────────────
    RegisterStub("sceAudioOutOpen",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutOpen(ctx); });
    RegisterStub("sceAudioOutOutput",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutOutput(ctx); });
    RegisterStub("sceAudioOutClose",
        [this](const SubsystemCallCtx& ctx) { return StubAudioOutClose(ctx); });
    RegisterStub("sceAudioOutGetLastOutputTimestamp",
        [this](const SubsystemCallCtx& ctx) {
            return StubAudioOutGetLastOutputTimestamp(ctx); });
    RegisterStub("sceAudio3dInitialize",
        [this](const SubsystemCallCtx& ctx) { return StubAudio3dInitialize(ctx); });
    RegisterStub("sceAudio3dCreateEmitter",
        [this](const SubsystemCallCtx& ctx) { return StubAudio3dCreateEmitter(ctx); });
    RegisterStub("sceAudio3dSetPosition",
        [this](const SubsystemCallCtx& ctx) { return StubAudio3dSetPosition(ctx); });
    RegisterStub("sceVoiceInit",
        [this](const SubsystemCallCtx& ctx) { return StubVoiceInit(ctx); });
    RegisterStub("sceVoiceStart",
        [this](const SubsystemCallCtx& ctx) { return StubVoiceStart(ctx); });
    RegisterStub("sceVoiceStop",
        [this](const SubsystemCallCtx& ctx) { return StubVoiceStop(ctx); });

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

    // ── User Service, PSN & Trophies ──────────────────────────────────────────
    RegisterStub("sceUserServiceInitialize",
        [this](const SubsystemCallCtx& ctx) { return StubUserServiceInitialize(ctx); });
    RegisterStub("sceUserServiceGetInitialUser",
        [this](const SubsystemCallCtx& ctx) { return StubUserServiceGetInitialUser(ctx); });
    RegisterStub("sceNpInitialize",
        [this](const SubsystemCallCtx& ctx) { return StubNpInitialize(ctx); });
    RegisterStub("sceNpGetOnlineStatus",
        [this](const SubsystemCallCtx& ctx) { return StubNpGetOnlineStatus(ctx); });
    RegisterStub("sceNpTrophyRegisterContext",
        [this](const SubsystemCallCtx& ctx) { return StubNpTrophyRegisterContext(ctx); });
    RegisterStub("sceNpTrophyUnlock",
        [this](const SubsystemCallCtx& ctx) { return StubNpTrophyUnlock(ctx); });

    // ── Dynamic System Modules & Debugging ────────────────────────────────────
    RegisterStub("sceSysmoduleLoadModule",
        [this](const SubsystemCallCtx& ctx) { return StubSysmoduleLoadModule(ctx); });
    RegisterStub("sceSysmoduleUnloadModule",
        [this](const SubsystemCallCtx& ctx) { return StubSysmoduleUnloadModule(ctx); });
    RegisterStub("sceSysmoduleIsLoaded",
        [this](const SubsystemCallCtx& ctx) { return StubSysmoduleIsLoaded(ctx); });
    RegisterStub("sceKernelDbgBreak",
        [this](const SubsystemCallCtx& ctx) { return StubKernelDbgBreak(ctx); });
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

Loader::SymbolResolverFn OpenOrbisSubsystemHub::CreateSymbolResolver(uint64_t stub_base_vaddr) {
    return [this, stub_base_vaddr](const std::string& name, const std::string& /*library*/) -> uint64_t {
        if (FindStub(name) != nullptr) {
            std::hash<std::string> hasher;
            uint64_t hash_offset = ((hasher(name) & 0x0000FFFF) + 1) * 16;
            return stub_base_vaddr + hash_offset;
        }
        return 0;
    };
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
int64_t OpenOrbisSubsystemHub::StubPadSetActuatorEffect(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadSetActuatorEffect(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadSetLightBar(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadSetLightBar(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadSetTriggerEffect(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadSetTriggerEffect(handle=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadGetHandle(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadGetHandle(userId=%llu, type=%llu) -> 1\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return 1;
}
int64_t OpenOrbisSubsystemHub::StubPadGetMotionData(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadGetMotionData(handle=%llu)\n", (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg1), 0, 64);
    }
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubPadGetTouchData(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] scePadGetTouchData(handle=%llu)\n", (unsigned long long)ctx.arg0);
    if (ctx.arg1 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg1), 0, 48);
    }
    return SCE_OK;
}

// ─── Common Dialogs & IME stubs ───────────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubCommonDialogInitialize(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceCommonDialogInitialize() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubMsgDialogOpen(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceMsgDialogOpen() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubMsgDialogGetStatus(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceMsgDialogGetStatus() -> FINISHED\n");
    return 2; // Finished
}
int64_t OpenOrbisSubsystemHub::StubMsgDialogClose(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceMsgDialogClose() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubImeDialogOpen(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceImeDialogOpen() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubImeDialogGetResult(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceImeDialogGetResult()\n");
    if (ctx.arg0 != 0) {
        std::memset(reinterpret_cast<void*>(ctx.arg0), 0, 64);
    }
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
int64_t OpenOrbisSubsystemHub::StubAudio3dInitialize(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceAudio3dInitialize() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubAudio3dCreateEmitter(const SubsystemCallCtx& /*ctx*/) {
    static uint32_t next_emitter_id = 1;
    uint32_t emitter = next_emitter_id++;
    LOGF("[SubsystemHub] sceAudio3dCreateEmitter() -> %u\n", emitter);
    return emitter;
}
int64_t OpenOrbisSubsystemHub::StubAudio3dSetPosition(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceAudio3dSetPosition(emitter=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubVoiceInit(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceVoiceInit() -> SCE_OK\n");
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubVoiceStart(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceVoiceStart(port=%llu)\n", (unsigned long long)ctx.arg0);
    return SCE_OK;
}
int64_t OpenOrbisSubsystemHub::StubVoiceStop(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceVoiceStop(port=%llu)\n", (unsigned long long)ctx.arg0);
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

// ─── User Service, PSN & Trophy stubs ────────────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubUserServiceInitialize(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceUserServiceInitialize() -> SCE_OK\n");
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubUserServiceGetInitialUser(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceUserServiceGetInitialUser() -> userId=0x10000000\n");
    if (ctx.arg0 != 0) {
        int32_t user_id = 0x10000000; // Standard primary local user ID
        std::memcpy(reinterpret_cast<void*>(ctx.arg0), &user_id, sizeof(user_id));
    }
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubNpInitialize(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceNpInitialize() -> SCE_OK\n");
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubNpGetOnlineStatus(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNpGetOnlineStatus() -> ONLINE\n");
    if (ctx.arg0 != 0) {
        int32_t status = 1; // Online / Signed In
        std::memcpy(reinterpret_cast<void*>(ctx.arg0), &status, sizeof(status));
    }
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubNpTrophyRegisterContext(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNpTrophyRegisterContext(context=%llu) -> handle=1\n",
         (unsigned long long)ctx.arg0);
    return 1; // Valid context handle
}

int64_t OpenOrbisSubsystemHub::StubNpTrophyUnlock(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceNpTrophyUnlock(handle=%llu, trophyId=%llu) -> UNLOCKED\n",
         (unsigned long long)ctx.arg0, (unsigned long long)ctx.arg1);
    return SCE_OK;
}

// ─── Dynamic System Modules & Debugging stubs ─────────────────────────────────

int64_t OpenOrbisSubsystemHub::StubSysmoduleLoadModule(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceSysmoduleLoadModule(id=0x%llx) -> SCE_OK\n",
         (unsigned long long)ctx.arg0);
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubSysmoduleUnloadModule(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceSysmoduleUnloadModule(id=0x%llx) -> SCE_OK\n",
         (unsigned long long)ctx.arg0);
    return SCE_OK;
}

int64_t OpenOrbisSubsystemHub::StubSysmoduleIsLoaded(const SubsystemCallCtx& ctx) {
    LOGF("[SubsystemHub] sceSysmoduleIsLoaded(id=0x%llx) -> 1 (LOADED)\n",
         (unsigned long long)ctx.arg0);
    return 1; // Loaded
}

int64_t OpenOrbisSubsystemHub::StubKernelDbgBreak(const SubsystemCallCtx& /*ctx*/) {
    LOGF("[SubsystemHub] sceKernelDbgBreak() -> SCE_OK\n");
    return SCE_OK;
}

} // namespace Kernel
