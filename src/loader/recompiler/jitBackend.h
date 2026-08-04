// jitBackend.h
//
// Architecture-Independent JIT Backend Abstraction Layer & Factory.

#ifndef LOADER_RECOMPILER_JIT_BACKEND_H
#define LOADER_RECOMPILER_JIT_BACKEND_H

#include "common/common.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Loader::Recompiler {

enum class JitBackendType : uint8_t {
	Arm64 = 0,
	X86_64
};

class IJitBackend {
public:
	virtual ~IJitBackend() = default;

	[[nodiscard]] virtual JitBackendType GetBackendType() const noexcept = 0;
	[[nodiscard]] virtual bool CompileBlock(const uint8_t* x86_bytes, size_t size, uint64_t guest_rip) = 0;
	[[nodiscard]] virtual CompiledBlockFunc LookupBlock(uint64_t guest_rip) = 0;
	virtual void Reset() = 0;
};


class JitBackendFactory {
public:
	[[nodiscard]] static std::unique_ptr<IJitBackend> CreateBackend(JitBackendType type);
	[[nodiscard]] static JitBackendType GetDefaultBackendType() noexcept;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_BACKEND_H
