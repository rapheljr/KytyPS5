// jitBackend.cpp
//
// Architecture-Independent JIT Backend Abstraction Layer & Factory.

#include "loader/recompiler/jitBackend.h"

namespace Loader::Recompiler {

class Arm64JitBackendImpl : public IJitBackend {
public:
	Arm64JitBackendImpl() = default;
	~Arm64JitBackendImpl() override = default;

	[[nodiscard]] JitBackendType GetBackendType() const noexcept override {
		return JitBackendType::Arm64;
	}

	bool CompileBlock(const uint8_t* x86_bytes, size_t size, uint64_t guest_rip) override {
		return m_bridge.CompileAndCacheBlock(x86_bytes, size, guest_rip);
	}

	CompiledBlockFunc LookupBlock(uint64_t guest_rip) override {
		return m_bridge.GetBlockCache().Lookup(guest_rip);
	}

	void Reset() override {
		m_bridge.GetBlockCache().Clear();
	}

private:
	X86RuntimeBridge m_bridge;
};

class X86_64JitBackendImpl : public IJitBackend {
public:
	X86_64JitBackendImpl() = default;
	~X86_64JitBackendImpl() override = default;

	[[nodiscard]] JitBackendType GetBackendType() const noexcept override {
		return JitBackendType::X86_64;
	}

	bool CompileBlock(const uint8_t* x86_bytes, size_t size, uint64_t guest_rip) override {
		return m_bridge.CompileAndCacheBlock(x86_bytes, size, guest_rip);
	}

	CompiledBlockFunc LookupBlock(uint64_t guest_rip) override {
		return m_bridge.GetBlockCache().Lookup(guest_rip);
	}


	void Reset() override {
		m_bridge.GetBlockCache().Clear();
	}

private:
	X86RuntimeBridge m_bridge;
};

std::unique_ptr<IJitBackend> JitBackendFactory::CreateBackend(JitBackendType type) {
	if (type == JitBackendType::Arm64) {
		return std::make_unique<Arm64JitBackendImpl>();
	} else {
		return std::make_unique<X86_64JitBackendImpl>();
	}
}

JitBackendType JitBackendFactory::GetDefaultBackendType() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
	return JitBackendType::Arm64;
#else
	return JitBackendType::X86_64;
#endif
}

} // namespace Loader::Recompiler
