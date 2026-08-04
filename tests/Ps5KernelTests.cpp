// Ps5KernelTests.cpp
//
// Unit, multi-threaded synchronization, VFS, timer, and benchmark test suite for Phase N:
// PS5 Kernel Emulation.

#include "kernel/ps5Kernel.h"
#include "kernel/ps5Network.h"
#include "kernel/ps5Sync.h"
#include "kernel/ps5Vfs.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Libs::Kernel::Ps5;

// ─── 1. Syscall Dispatcher & Thread Manager Test ────────────────────────────────

void TestSyscallDispatcherAndThreadManager() {
	std::printf("  [Test 1] Syscall Dispatcher & Thread Manager...\n");

	SyscallDispatcher dispatcher;
	dispatcher.RegisterSyscall(100, [](uint64_t a0, uint64_t a1, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return static_cast<int64_t>(a0 + a1);
	});

	int64_t res = dispatcher.Dispatch(100, 30, 70, 0, 0, 0, 0);
	Check(res == 100, "Syscall dispatcher result mismatch");

	int64_t invalid_res = dispatcher.Dispatch(999, 0, 0, 0, 0, 0, 0);
	Check(invalid_res == -1, "Invalid syscall should return -1");

	ThreadManager thread_mgr;
	uint32_t tid = thread_mgr.CreateThread("MainWorker", 256, 1024 * 1024);
	Check(tid > 0, "Thread creation failed");

	bool started = thread_mgr.StartThread(tid);
	Check(started, "Thread start failed");
	Check(thread_mgr.GetActiveThreadCount() == 1, "Active thread count mismatch");

	thread_mgr.TerminateThread(tid);
	Check(thread_mgr.GetActiveThreadCount() == 0, "Terminated thread count mismatch");

	std::printf("  [OK] Test 1: Syscall Dispatcher & Thread Manager\n");
}

// ─── 2. Multi-Threaded Synchronization Stress Test (Mutex & Semaphore) ──────

void TestKernelSynchronization() {
	std::printf("  [Test 2] Multi-Threaded Synchronization (Mutex, Semaphore, RwLock)...\n");

	// 1. Mutex contention test
	KernelMutex mutex("TestMutex");
	uint64_t shared_counter = 0;

	constexpr int kThreads = 8;
	constexpr int kIncrementsPerThread = 10000;

	std::vector<std::thread> threads;
	for (int i = 0; i < kThreads; ++i) {
		threads.emplace_back([&mutex, &shared_counter]() {
			for (int j = 0; j < kIncrementsPerThread; ++j) {
				mutex.Lock();
				shared_counter++;
				mutex.Unlock();
			}
		});
	}
	for (auto& t : threads) t.join();
	Check(shared_counter == static_cast<uint64_t>(kThreads * kIncrementsPerThread), "Mutex shared counter race detected");

	// 2. Semaphore test
	KernelSemaphore sem("TestSem", 2, 2);
	Check(sem.GetCount() == 2, "Sem initial count mismatch");
	sem.Wait();
	Check(sem.GetCount() == 1, "Sem count after wait mismatch");
	sem.Signal();
	Check(sem.GetCount() == 2, "Sem count after signal mismatch");

	// 3. RW Lock test
	KernelRwLock rw_lock("TestRwLock");
	rw_lock.LockRead();
	rw_lock.UnlockRead();
	rw_lock.LockWrite();
	rw_lock.UnlockWrite();

	std::printf("  [OK] Test 2: Multi-Threaded Synchronization\n");
}

// ─── 3. Virtual Filesystem & Package Mount Test ────────────────────────────────

void TestVirtualFileSystem() {
	std::printf("  [Test 3] Virtual Filesystem & Mount Point Resolution...\n");

	VirtualFileSystem vfs;
	vfs.Mount("/app0", "/host/game/data");
	vfs.Mount("/savedata", "/host/user/saves");
	vfs.Mount("/temp", "/host/tmp");

	Check(vfs.GetMountPointCount() == 3, "VFS mount count mismatch");

	std::string resolved_app = vfs.ResolvePath("/app0/eboot.bin");
	Check(resolved_app == "/host/game/data/eboot.bin", "App0 mount path resolution mismatch");

	std::string resolved_save = vfs.ResolvePath("/savedata/save0.dat");
	Check(resolved_save == "/host/user/saves/save0.dat", "SaveData mount path resolution mismatch");

	int32_t fd = vfs.OpenFile("/app0/eboot.bin", VfsOpenFlags::ReadOnly);
	Check(fd >= 10, "VFS open file descriptor invalid");
	vfs.CloseFile(fd);

	std::printf("  [OK] Test 3: Virtual Filesystem & Mount Point Resolution\n");
}

// ─── 4. High-Res Clock & Network Sockets Test ────────────────────────────────

void TestTimersAndSockets() {
	std::printf("  [Test 4] High-Resolution Clocks & Network Sockets...\n");

	KernelTimeSpec ts1 = KernelClock::GetTimeNanoseconds();
	KernelClock::SleepMicroseconds(1000); // 1 ms sleep
	KernelTimeSpec ts2 = KernelClock::GetTimeNanoseconds();

	Check(ts2.sec > ts1.sec || (ts2.sec == ts1.sec && ts2.nsec > ts1.nsec), "Clock monotonically increasing failed");

	NetworkManager net_mgr;
	int32_t sock_fd = net_mgr.CreateSocket(SocketType::Stream);
	Check(sock_fd >= 100, "Socket creation failed");

	bool bound = net_mgr.Bind(sock_fd, "127.0.0.1", 8080);
	Check(bound, "Socket bind failed");

	net_mgr.CloseSocket(sock_fd);
	Check(net_mgr.GetActiveSocketCount() == 0, "Socket close active count mismatch");

	std::printf("  [OK] Test 4: High-Resolution Clocks & Network Sockets\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkPs5Kernel() {
	std::printf("\n--- Phase N Benchmarks ---\n");

	// 1. Syscall Dispatcher Latency Benchmark
	SyscallDispatcher dispatcher;
	dispatcher.RegisterSyscall(42, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0;
	});

	constexpr int kSyscallBatch = 1000000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kSyscallBatch; ++i) {
		dispatcher.Dispatch(42, 0, 0, 0, 0, 0, 0);
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double dispatch_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kSyscallBatch;
	double dispatch_throughput = kSyscallBatch / std::chrono::duration<double>(t1 - t0).count();
	std::printf("  [Bench] Syscall Dispatch Latency: %.2f ns / syscall (Throughput: %.2f M syscalls/sec)\n",
	           dispatch_dt_ns, dispatch_throughput / 1e6);

	// 2. Mutex Lock/Unlock Latency Benchmark
	KernelMutex mutex("BenchMutex");
	constexpr int kMutexOps = 1000000;
	t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kMutexOps; ++i) {
		mutex.Lock();
		mutex.Unlock();
	}
	t1 = std::chrono::high_resolution_clock::now();

	double mutex_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kMutexOps;
	double mutex_throughput = kMutexOps / std::chrono::duration<double>(t1 - t0).count();
	std::printf("  [Bench] KernelMutex Lock/Unlock Latency: %.2f ns / op (Throughput: %.2f M ops/sec)\n",
	           mutex_dt_ns, mutex_throughput / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase N: PS5 Kernel Emulation              \n");
	std::printf("====================================================\n\n");

	TestSyscallDispatcherAndThreadManager();
	TestKernelSynchronization();
	TestVirtualFileSystem();
	TestTimersAndSockets();

	BenchmarkPs5Kernel();

	std::printf("\nPs5KernelTests: ALL PASSED\n");
	return 0;
}
