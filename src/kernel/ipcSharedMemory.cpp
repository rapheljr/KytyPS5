// ipcSharedMemory.cpp
//
// Pipes, Socketpair, POSIX Message Queues & Shared Memory Implementation.

#include "kernel/ipcSharedMemory.h"

#include <algorithm>
#include <cstring>

namespace Libs::Kernel::Ps5 {

// ─── PipeObject ───────────────────────────────────────────────────────────────

int64_t PipeObject::Read(void* buf, size_t count, bool non_blocking) {
	if (!buf || count == 0) return 0;

	std::unique_lock<std::mutex> lock(m_mutex);
	while (m_buffer.empty()) {
		if (!m_write_open) {
			return 0; // EOF
		}
		if (non_blocking) {
			return -1; // EAGAIN
		}
		m_cv_read.wait(lock);
	}

	size_t read_bytes = std::min(count, m_buffer.size());
	uint8_t* dst = static_cast<uint8_t*>(buf);
	for (size_t i = 0; i < read_bytes; ++i) {
		dst[i] = m_buffer.front();
		m_buffer.pop_front();
	}

	m_cv_write.notify_one();
	return static_cast<int64_t>(read_bytes);
}

int64_t PipeObject::Write(const void* buf, size_t count, bool non_blocking) {
	if (!buf || count == 0) return 0;

	std::unique_lock<std::mutex> lock(m_mutex);
	if (!m_read_open) {
		return -1; // EPIPE
	}

	while (m_buffer.size() >= m_capacity) {
		if (non_blocking) {
			return -1; // EAGAIN
		}
		m_cv_write.wait(lock);
		if (!m_read_open) {
			return -1; // EPIPE
		}
	}

	size_t available_space = m_capacity - m_buffer.size();
	size_t write_bytes = std::min(count, available_space);
	const uint8_t* src = static_cast<const uint8_t*>(buf);

	for (size_t i = 0; i < write_bytes; ++i) {
		m_buffer.push_back(src[i]);
	}

	m_cv_read.notify_one();
	return static_cast<int64_t>(write_bytes);
}

void PipeObject::CloseRead() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_read_open = false;
	m_cv_write.notify_all();
}

void PipeObject::CloseWrite() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_write_open = false;
	m_cv_read.notify_all();
}

size_t PipeObject::GetAvailableReadBytes() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_buffer.size();
}

// ─── PipeManager ──────────────────────────────────────────────────────────────

bool PipeManager::CreatePipe(int32_t fds[2], uint32_t owner_pid, int32_t /*flags*/) {
	if (!fds) return false;
	auto pipe = std::make_shared<PipeObject>("AnonymousPipe", owner_pid);
	fds[0] = 50; // Read FD placeholder
	fds[1] = 51; // Write FD placeholder
	return true;
}

bool PipeManager::CreateSocketPair(int32_t fds[2], uint32_t owner_pid) {
	return CreatePipe(fds, owner_pid);
}

// ─── SharedMemoryManager ──────────────────────────────────────────────────────

int32_t SharedMemoryManager::ShmOpen(const std::string& name, int32_t /*flags*/, uint32_t /*mode*/, uint32_t owner_pid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_shm_map.find(name);
	if (it != m_shm_map.end()) {
		return static_cast<int32_t>(reinterpret_cast<uintptr_t>(it->second.get()) & 0x7FFFFFFF);
	}

	auto shm = std::make_shared<SharedMemoryObject>(name, 4096, owner_pid);
	m_shm_map[name] = shm;
	return static_cast<int32_t>(reinterpret_cast<uintptr_t>(shm.get()) & 0x7FFFFFFF);
}

bool SharedMemoryManager::ShmUnlink(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_shm_map.find(name);
	if (it == m_shm_map.end()) {
		return false;
	}
	m_shm_map.erase(it);
	return true;
}

SharedMemoryRef SharedMemoryManager::GetShm(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_shm_map.find(name);
	if (it == m_shm_map.end()) {
		return nullptr;
	}
	return it->second;
}

size_t SharedMemoryManager::GetActiveShmCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_shm_map.size();
}

// ─── MessageQueue ─────────────────────────────────────────────────────────────

bool MessageQueue::Send(const void* buf, size_t len, uint32_t prio, bool non_blocking) {
	if (!buf || len > m_msg_size) return false;

	std::unique_lock<std::mutex> lock(m_mutex);
	while (m_messages.size() >= m_max_msg) {
		if (non_blocking) return false;
		m_cv_write.wait(lock);
	}

	MqMessage msg{};
	msg.prio = prio;
	msg.data.resize(len);
	std::memcpy(msg.data.data(), buf, len);

	// Insert ordered by priority (highest priority first)
	auto it = m_messages.begin();
	while (it != m_messages.end() && it->prio >= prio) {
		++it;
	}
	m_messages.insert(it, std::move(msg));

	m_cv_read.notify_one();
	return true;
}

int64_t MessageQueue::Receive(void* buf, size_t len, uint32_t* prio_out, bool non_blocking) {
	if (!buf || len == 0) return 0;

	std::unique_lock<std::mutex> lock(m_mutex);
	while (m_messages.empty()) {
		if (non_blocking) return -1;
		m_cv_read.wait(lock);
	}

	auto msg = std::move(m_messages.front());
	m_messages.pop_front();

	size_t copy_bytes = std::min(len, msg.data.size());
	std::memcpy(buf, msg.data.data(), copy_bytes);

	if (prio_out) {
		*prio_out = msg.prio;
	}

	m_cv_write.notify_one();
	return static_cast<int64_t>(copy_bytes);
}

size_t MessageQueue::GetMessageCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_messages.size();
}

// ─── MessageQueueManager ──────────────────────────────────────────────────────

int32_t MessageQueueManager::MqOpen(const std::string& name, int32_t /*flags*/, size_t max_msg, size_t msg_size) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_named_mqs.find(name);
	MessageQueueRef mq;
	if (it != m_named_mqs.end()) {
		mq = it->second;
	} else {
		mq = std::make_shared<MessageQueue>(name, max_msg, msg_size);
		m_named_mqs[name] = mq;
	}

	int32_t mqdes = m_next_mq_id++;
	m_mq_handles[mqdes] = mq;
	return mqdes;
}

bool MessageQueueManager::MqClose(int32_t mqdes) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_mq_handles.find(mqdes);
	if (it == m_mq_handles.end()) {
		return false;
	}
	m_mq_handles.erase(it);
	return true;
}

bool MessageQueueManager::MqSend(int32_t mqdes, const void* buf, size_t len, uint32_t prio) {
	MessageQueueRef mq;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_mq_handles.find(mqdes);
		if (it == m_mq_handles.end()) return false;
		mq = it->second;
	}
	return mq->Send(buf, len, prio);
}

int64_t MessageQueueManager::MqReceive(int32_t mqdes, void* buf, size_t len, uint32_t* prio_out) {
	MessageQueueRef mq;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_mq_handles.find(mqdes);
		if (it == m_mq_handles.end()) return -1;
		mq = it->second;
	}
	return mq->Receive(buf, len, prio_out);
}

} // namespace Libs::Kernel::Ps5
