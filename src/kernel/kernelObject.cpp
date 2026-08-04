// kernelObject.cpp
//
// Unified Thread-Safe Kernel Object & Handle Table Manager for PS5 Kernel Emulation.

#include "kernel/kernelObject.h"

namespace Libs::Kernel::Ps5 {

HandleTable::HandleTable() = default;

HandleTable::HandleTable(HandleTable&& other) noexcept {
	std::lock_guard<std::mutex> lock(other.m_mutex);
	m_next_handle = other.m_next_handle;
	m_handles     = std::move(other.m_handles);
}

HandleTable& HandleTable::operator=(HandleTable&& other) noexcept {
	if (this != &other) {
		std::unique_lock<std::mutex> lock1(m_mutex, std::defer_lock);
		std::unique_lock<std::mutex> lock2(other.m_mutex, std::defer_lock);
		std::lock(lock1, lock2);
		m_next_handle = other.m_next_handle;
		m_handles     = std::move(other.m_handles);
	}
	return *this;
}

handle_t HandleTable::AllocateHandle(KernelObjectRef object, uint32_t flags) {
	if (!object) return kInvalidHandle;

	std::lock_guard<std::mutex> lock(m_mutex);
	handle_t handle = m_next_handle++;
	object->SetHandle(handle);
	m_handles[handle] = HandleEntry{object, flags};
	return handle;
}

bool HandleTable::FreeHandle(handle_t handle) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_handles.find(handle);
	if (it == m_handles.end()) {
		return false;
	}
	m_handles.erase(it);
	return true;
}

KernelObjectRef HandleTable::Lookup(handle_t handle, KernelObjectType expected_type) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_handles.find(handle);
	if (it == m_handles.end()) {
		return nullptr;
	}
	if (expected_type != KernelObjectType::Unknown && it->second.object->GetType() != expected_type) {
		return nullptr;
	}
	return it->second.object;
}

bool HandleTable::IsValid(handle_t handle) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_handles.find(handle) != m_handles.end();
}

void HandleTable::Clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_handles.clear();
}

size_t HandleTable::GetActiveHandleCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_handles.size();
}

} // namespace Libs::Kernel::Ps5
