// machExceptionHandler.cpp
//
// Apple Silicon Native Darwin Mach Exception Server & Fault Recovery Engine.

#include "kernel/machExceptionHandler.h"

#include <cstdio>
#include <cstring>
#include <chrono>

namespace Kernel {

MachExceptionHandler::MachExceptionHandler() = default;

MachExceptionHandler::~MachExceptionHandler() {
	Shutdown();
}

bool MachExceptionHandler::Initialize() {
	if (m_active.load()) {
		return true;
	}

#if defined(__APPLE__)
	kern_return_t kr = mach_port_allocate(
		mach_task_self(),
		MACH_PORT_RIGHT_RECEIVE,
		&m_exception_port);

	if (kr != KERN_SUCCESS) {
		std::printf("[MachExceptionHandler] mach_port_allocate failed: %d\n", kr);
		return false;
	}

	kr = mach_port_insert_right(
		mach_task_self(),
		m_exception_port,
		m_exception_port,
		MACH_MSG_TYPE_MAKE_SEND);

	if (kr != KERN_SUCCESS) {
		mach_port_deallocate(mach_task_self(), m_exception_port);
		m_exception_port = MACH_PORT_NULL;
		std::printf("[MachExceptionHandler] mach_port_insert_right failed: %d\n", kr);
		return false;
	}

	m_active.store(true);
	m_server_thread = std::thread(&MachExceptionHandler::ServerThreadMain, this);
	return true;
#else
	m_active.store(true);
	return true;
#endif
}

void MachExceptionHandler::Shutdown() {
	if (!m_active.exchange(false)) {
		return;
	}

#if defined(__APPLE__)
	if (m_exception_port != MACH_PORT_NULL) {
		// Send a dummy message or deallocate to unblock server thread
		mach_port_destroy(mach_task_self(), m_exception_port);
		m_exception_port = MACH_PORT_NULL;
	}
#endif

	if (m_server_thread.joinable()) {
		m_server_thread.join();
	}
}

bool MachExceptionHandler::InstallThreadHandler(uint64_t mach_thread_port) {
	if (!m_active.load()) return false;

#if defined(__APPLE__)
	mach_port_t thread_port = static_cast<mach_port_t>(mach_thread_port);
	if (thread_port == MACH_PORT_NULL) {
		thread_port = mach_thread_self();
	}

	kern_return_t kr = thread_set_exception_ports(
		thread_port,
		EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION,
		m_exception_port,
		static_cast<exception_behavior_t>(EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES),
		ARM_THREAD_STATE64);

	return (kr == KERN_SUCCESS);
#else
	(void)mach_thread_port;
	return true;
#endif
}

bool MachExceptionHandler::RemoveThreadHandler(uint64_t mach_thread_port) {
#if defined(__APPLE__)
	mach_port_t thread_port = static_cast<mach_port_t>(mach_thread_port);
	if (thread_port == MACH_PORT_NULL) {
		thread_port = mach_thread_self();
	}

	kern_return_t kr = thread_set_exception_ports(
		thread_port,
		EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION,
		MACH_PORT_NULL,
		EXCEPTION_DEFAULT,
		THREAD_STATE_NONE);

	return (kr == KERN_SUCCESS);
#else
	(void)mach_thread_port;
	return true;
#endif
}

void MachExceptionHandler::ServerThreadMain() {
#if defined(__APPLE__)
	// Mach message buffer structures
	struct RequestMsg {
		mach_msg_header_t head;
		mach_msg_body_t   msgh_body;
		mach_msg_port_descriptor_t thread;
		mach_msg_port_descriptor_t task;
		NDR_record_t      NDR;
		exception_type_t  exception;
		mach_msg_type_number_t codeCnt;
		int64_t           code[2];
		int               flavor;
		mach_msg_type_number_t old_stateCnt;
		natural_t         old_state[ARM_THREAD_STATE64_COUNT];
		uint8_t           pad[256];
	} req{};

	struct ReplyMsg {
		mach_msg_header_t head;
		NDR_record_t      NDR;
		kern_return_t     RetCode;
		int               flavor;
		mach_msg_type_number_t new_stateCnt;
		natural_t         new_state[ARM_THREAD_STATE64_COUNT];
	} rep{};

	while (m_active.load()) {
		std::memset(&req, 0, sizeof(req));
		mach_msg_return_t mr = mach_msg(
			&req.head,
			MACH_RCV_MSG | MACH_RCV_TIMEOUT,
			0,
			sizeof(req),
			m_exception_port,
			100, // 100ms timeout
			MACH_PORT_NULL);

		if (mr == MACH_RCV_TIMED_OUT) {
			continue;
		}
		if (mr != MACH_MSG_SUCCESS) {
			if (!m_active.load()) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		MachExceptionContext ctx{};
		if (req.exception == EXC_BAD_ACCESS) {
			ctx.type = MachExceptionType::BadAccess;
			if (req.codeCnt >= 2) {
				ctx.fault_address = static_cast<uint64_t>(req.code[1]);
			}
		} else if (req.exception == EXC_BAD_INSTRUCTION) {
			ctx.type = MachExceptionType::BadInstruction;
		}

		ctx.thread_id = req.thread.name;

		if (req.flavor == ARM_THREAD_STATE64 && req.old_stateCnt >= ARM_THREAD_STATE64_COUNT) {
			auto* state = reinterpret_cast<arm_thread_state64_t*>(req.old_state);
			ctx.pc = arm_thread_state64_get_pc(*state);
			ctx.lr = arm_thread_state64_get_lr(*state);
			ctx.sp = arm_thread_state64_get_sp(*state);
		}

		bool handled = false;
		if (m_callback) {
			handled = m_callback(ctx);
		}

		m_handled_count.fetch_add(1, std::memory_order_relaxed);

		// Reply to resume or fail
		std::memset(&rep, 0, sizeof(rep));
		rep.head.msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(req.head.msgh_bits), 0);
		rep.head.msgh_remote_port = req.head.msgh_remote_port;
		rep.head.msgh_local_port  = MACH_PORT_NULL;
		rep.head.msgh_size        = sizeof(rep);
		rep.head.msgh_id          = req.head.msgh_id + 100;
		rep.NDR                   = req.NDR;
		rep.RetCode               = handled ? KERN_SUCCESS : KERN_FAILURE;
		rep.flavor                = THREAD_STATE_NONE;
		rep.new_stateCnt          = 0;

		mach_msg(
			&rep.head,
			MACH_SEND_MSG,
			sizeof(rep),
			0,
			MACH_PORT_NULL,
			MACH_MSG_TIMEOUT_NONE,
			MACH_PORT_NULL);
	}
#endif
}

} // namespace Kernel
