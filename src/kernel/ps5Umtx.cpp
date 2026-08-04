// ps5Umtx.cpp
//
// FreeBSD _umtx_op Userland Synchronization Futex Engine Implementation.

#include "kernel/ps5Umtx.h"

#include <chrono>
#include <thread>

namespace Libs::Kernel::Ps5 {

int64_t UmtxManager::SysUmtxOp(uint64_t obj_ptr, uint32_t op, uint64_t val, uint64_t /*uaddr1*/, uint64_t /*uaddr2*/) {
	if (obj_ptr == 0) {
		return -1; // EINVAL
	}

	switch (op) {
		case UMTX_OP_LOCK:
		case UMTX_OP_MUTEX_WAIT:
		case UMTX_OP_WAIT:
		case UMTX_OP_CV_WAIT:
		case UMTX_OP_RW_RDLOCK:
		case UMTX_OP_RW_WRLOCK:
		case UMTX_OP_SEM_WAIT: {
			std::unique_lock<std::mutex> lock(m_mutex);
			uint32_t* host_val_ptr = reinterpret_cast<uint32_t*>(obj_ptr);

			// If val doesn't match expected atomic value, return EBUSY / EWOULDBLOCK
			if (host_val_ptr && *host_val_ptr != static_cast<uint32_t>(val) && op == UMTX_OP_WAIT) {
				return -2; // EBUSY / EWOULDBLOCK
			}

			UmtxWaiter waiter{};
			m_waiters[obj_ptr].push_back(&waiter);

			// Wait unlock and condition wait
			waiter.cv.wait(lock, [&waiter]() { return waiter.woken; });

			// Clean up waiter
			auto& queue = m_waiters[obj_ptr];
			for (auto it = queue.begin(); it != queue.end(); ++it) {
				if (*it == &waiter) {
					queue.erase(it);
					break;
				}
			}
			if (queue.empty()) {
				m_waiters.erase(obj_ptr);
			}

			return 0; // Success
		}

		case UMTX_OP_UNLOCK:
		case UMTX_OP_MUTEX_WAKE:
		case UMTX_OP_WAKE:
		case UMTX_OP_CV_SIGNAL:
		case UMTX_OP_RW_UNLOCK:
		case UMTX_OP_SEM_WAKE: {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_waiters.find(obj_ptr);
			if (it != m_waiters.end() && !it->second.empty()) {
				uint32_t wake_count = static_cast<uint32_t>(val > 0 ? val : 1);
				uint32_t woken = 0;

				for (auto* waiter : it->second) {
					if (!waiter->woken) {
						waiter->woken = true;
						waiter->cv.notify_one();
						woken++;
						if (woken >= wake_count) break;
					}
				}
				return static_cast<int64_t>(woken);
			}
			return 0; // No waiters
		}

		case UMTX_OP_CV_BROADCAST: {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_waiters.find(obj_ptr);
			if (it != m_waiters.end() && !it->second.empty()) {
				uint32_t woken = 0;
				for (auto* waiter : it->second) {
					if (!waiter->woken) {
						waiter->woken = true;
						waiter->cv.notify_one();
						woken++;
					}
				}
				return static_cast<int64_t>(woken);
			}
			return 0;
		}

		default:
			return -1; // ENOSYS / EINVAL
	}
}

size_t UmtxManager::GetActiveWaitersCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	size_t count = 0;
	for (const auto& [addr, waiters] : m_waiters) {
		count += waiters.size();
	}
	return count;
}

} // namespace Libs::Kernel::Ps5
