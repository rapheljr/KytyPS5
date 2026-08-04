// kernelObject.h
//
// Unified Thread-Safe Kernel Object & Handle Table Manager for PS5 Kernel Emulation.

#ifndef KERNEL_KERNEL_OBJECT_H
#define KERNEL_KERNEL_OBJECT_H

#include "common/common.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Libs::Kernel::Ps5 {

enum class KernelObjectType : uint8_t {
	Unknown = 0,
	Process,
	Thread,
	File,
	Mutex,
	Semaphore,
	RwLock,
	Cond,
	EventFlag,
	EventQueue,
	SharedMemory,
	Timer,
	Socket,
	Pipe
};

using handle_t = int32_t;
constexpr handle_t kInvalidHandle = -1;

class KernelObject {
public:
	KernelObject(KernelObjectType type, const std::string& name = "", uint32_t owner_pid = 1)
	    : m_type(type), m_name(name), m_owner_pid(owner_pid) {}
	virtual ~KernelObject() = default;

	KYTY_CLASS_NO_COPY(KernelObject);

	[[nodiscard]] KernelObjectType GetType() const noexcept { return m_type; }
	[[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
	[[nodiscard]] uint32_t GetOwnerPid() const noexcept { return m_owner_pid; }
	[[nodiscard]] handle_t GetHandle() const noexcept { return m_handle; }

	void SetHandle(handle_t handle) noexcept { m_handle = handle; }

private:
	KernelObjectType m_type = KernelObjectType::Unknown;
	std::string      m_name;
	uint32_t         m_owner_pid = 1;
	handle_t         m_handle    = kInvalidHandle;
};

using KernelObjectRef = std::shared_ptr<KernelObject>;

struct HandleEntry {
	KernelObjectRef object;
	uint32_t        flags = 0; // Read, Write, NonBlocking, etc.
};

class HandleTable {
public:
	HandleTable();
	~HandleTable() = default;

	HandleTable(HandleTable&& other) noexcept;
	HandleTable& operator=(HandleTable&& other) noexcept;
	HandleTable(const HandleTable&) = delete;
	HandleTable& operator=(const HandleTable&) = delete;

	handle_t AllocateHandle(KernelObjectRef object, uint32_t flags = 0);
	bool FreeHandle(handle_t handle);
	[[nodiscard]] KernelObjectRef Lookup(handle_t handle, KernelObjectType expected_type = KernelObjectType::Unknown);
	[[nodiscard]] bool IsValid(handle_t handle) const;

	void Clear();
	[[nodiscard]] size_t GetActiveHandleCount() const;

private:
	mutable std::mutex                       m_mutex;
	handle_t                                 m_next_handle = 10; // Start FDs/Handles past std streams
	std::unordered_map<handle_t, HandleEntry> m_handles;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_KERNEL_OBJECT_H
