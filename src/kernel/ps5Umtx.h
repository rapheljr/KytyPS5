// ps5Umtx.h
//
// FreeBSD _umtx_op Userland Synchronization Futex Engine for PS5 Kernel Emulation.

#ifndef KERNEL_PS5_UMTX_H
#define KERNEL_PS5_UMTX_H

#include "common/common.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Libs::Kernel::Ps5 {

constexpr uint32_t UMTX_OP_LOCK          = 0;
constexpr uint32_t UMTX_OP_UNLOCK        = 1;
constexpr uint32_t UMTX_OP_WAIT          = 2;
constexpr uint32_t UMTX_OP_WAKE          = 3;
constexpr uint32_t UMTX_OP_MUTEX_WAIT    = 4;
constexpr uint32_t UMTX_OP_MUTEX_WAKE    = 5;
constexpr uint32_t UMTX_OP_CV_WAIT       = 6;
constexpr uint32_t UMTX_OP_CV_SIGNAL     = 7;
constexpr uint32_t UMTX_OP_CV_BROADCAST  = 8;
constexpr uint32_t UMTX_OP_RW_RDLOCK     = 9;
constexpr uint32_t UMTX_OP_RW_WRLOCK     = 10;
constexpr uint32_t UMTX_OP_RW_UNLOCK     = 11;
constexpr uint32_t UMTX_OP_SEM_WAIT      = 12;
constexpr uint32_t UMTX_OP_SEM_WAKE      = 13;

class UmtxManager {
public:
	UmtxManager() = default;
	~UmtxManager() = default;

	KYTY_CLASS_NO_COPY(UmtxManager);

	int64_t SysUmtxOp(uint64_t obj_ptr, uint32_t op, uint64_t val, uint64_t uaddr1, uint64_t uaddr2);

	[[nodiscard]] size_t GetActiveWaitersCount() const;

private:
	struct UmtxWaiter {
		std::condition_variable cv;
		bool                    woken = false;
	};

	mutable std::mutex                                     m_mutex;
	std::unordered_map<uint64_t, std::vector<UmtxWaiter*>> m_waiters;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_UMTX_H
