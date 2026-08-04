// ipcSharedMemory.h
//
// Pipes, Socketpair, POSIX Message Queues & Shared Memory for PS5 Kernel Emulation.

#ifndef KERNEL_IPC_SHARED_MEMORY_H
#define KERNEL_IPC_SHARED_MEMORY_H

#include "common/common.h"
#include "kernel/kernelObject.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

// ─── Pipe ────────────────────────────────────────────────────────────────────

class PipeObject : public KernelObject {
public:
	explicit PipeObject(const std::string& name = "Pipe", uint32_t owner_pid = 1, size_t capacity = 65536)
	    : KernelObject(KernelObjectType::Pipe, name, owner_pid), m_capacity(capacity) {}
	~PipeObject() override = default;

	KYTY_CLASS_NO_COPY(PipeObject);

	int64_t Read(void* buf, size_t count, bool non_blocking = false);
	int64_t Write(const void* buf, size_t count, bool non_blocking = false);
	void CloseRead();
	void CloseWrite();

	[[nodiscard]] size_t GetAvailableReadBytes() const;

private:
	size_t                  m_capacity = 65536;
	bool                    m_read_open = true;
	bool                    m_write_open = true;
	std::deque<uint8_t>     m_buffer;
	mutable std::mutex      m_mutex;
	std::condition_variable m_cv_read;
	std::condition_variable m_cv_write;
};

using PipeObjectRef = std::shared_ptr<PipeObject>;

class PipeManager {
public:
	PipeManager() = default;
	~PipeManager() = default;

	KYTY_CLASS_NO_COPY(PipeManager);

	bool CreatePipe(int32_t fds[2], uint32_t owner_pid = 1, int32_t flags = 0);
	bool CreateSocketPair(int32_t fds[2], uint32_t owner_pid = 1);
};

// ─── Shared Memory ────────────────────────────────────────────────────────────

class SharedMemoryObject : public KernelObject {
public:
	SharedMemoryObject(const std::string& name, size_t size, uint32_t owner_pid = 1)
	    : KernelObject(KernelObjectType::SharedMemory, name, owner_pid), m_data(size, 0) {}
	~SharedMemoryObject() override = default;

	KYTY_CLASS_NO_COPY(SharedMemoryObject);

	void* GetPointer() noexcept { return m_data.data(); }
	[[nodiscard]] const void* GetPointer() const noexcept { return m_data.data(); }
	[[nodiscard]] size_t GetSize() const noexcept { return m_data.size(); }
	void Truncate(size_t new_size) { m_data.resize(new_size, 0); }

private:
	std::vector<uint8_t> m_data;
};

using SharedMemoryRef = std::shared_ptr<SharedMemoryObject>;

class SharedMemoryManager {
public:
	SharedMemoryManager() = default;
	~SharedMemoryManager() = default;

	KYTY_CLASS_NO_COPY(SharedMemoryManager);

	int32_t ShmOpen(const std::string& name, int32_t flags, uint32_t mode, uint32_t owner_pid = 1);
	bool ShmUnlink(const std::string& name);
	[[nodiscard]] SharedMemoryRef GetShm(const std::string& name);

	[[nodiscard]] size_t GetActiveShmCount() const;

private:
	mutable std::mutex                             m_mutex;
	std::unordered_map<std::string, SharedMemoryRef> m_shm_map;
};

// ─── Message Queue ────────────────────────────────────────────────────────────

struct MqMessage {
	uint32_t             prio = 0;
	std::vector<uint8_t> data;
};

class MessageQueue {
public:
	MessageQueue(const std::string& name, size_t max_msg, size_t msg_size)
	    : m_name(name), m_max_msg(max_msg), m_msg_size(msg_size) {}
	~MessageQueue() = default;

	KYTY_CLASS_NO_COPY(MessageQueue);

	bool Send(const void* buf, size_t len, uint32_t prio, bool non_blocking = false);
	int64_t Receive(void* buf, size_t len, uint32_t* prio_out, bool non_blocking = false);

	[[nodiscard]] size_t GetMessageCount() const;

private:
	std::string             m_name;
	size_t                  m_max_msg  = 10;
	size_t                  m_msg_size = 1024;
	std::deque<MqMessage>   m_messages;
	mutable std::mutex      m_mutex;
	std::condition_variable m_cv_read;
	std::condition_variable m_cv_write;
};

using MessageQueueRef = std::shared_ptr<MessageQueue>;

class MessageQueueManager {
public:
	MessageQueueManager() = default;
	~MessageQueueManager() = default;

	KYTY_CLASS_NO_COPY(MessageQueueManager);

	int32_t MqOpen(const std::string& name, int32_t flags, size_t max_msg, size_t msg_size);
	bool MqClose(int32_t mqdes);
	bool MqSend(int32_t mqdes, const void* buf, size_t len, uint32_t prio);
	int64_t MqReceive(int32_t mqdes, void* buf, size_t len, uint32_t* prio_out);

private:
	mutable std::mutex                            m_mutex;
	int32_t                                       m_next_mq_id = 100;
	std::unordered_map<std::string, MessageQueueRef> m_named_mqs;
	std::unordered_map<int32_t, MessageQueueRef>  m_mq_handles;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_IPC_SHARED_MEMORY_H
