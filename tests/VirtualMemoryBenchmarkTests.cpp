// VirtualMemoryBenchmarkTests.cpp
//
// Performance benchmark suite for the Virtual Memory subsystem:
//   - SysVirtualAlloc / SysVirtualFree throughput
//   - SysVirtualReserve / SysVirtualCommit / SysVirtualDecommit / SysVirtualFree lifecycle
//   - SysVirtualProtect state-change latency
//   - SysVirtualAllocAligned vs SysVirtualAlloc overhead
//   - Multi-threaded mutex contention benchmark (g_virtual_mutex)

#include "common/config.h"
#include "common/platform/sysVirtual.h"
#include "common/virtualMemory.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;

// ---------------------------------------------------------------------------
// Benchmark 1: Alloc / Free Throughput
// ---------------------------------------------------------------------------

void BenchmarkAllocFree(size_t iterations, size_t alloc_size) {
    const auto start = Clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t addr = Common::SysVirtualAlloc(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite);
        if (addr == 0) {
            std::fprintf(stderr, "BenchmarkAllocFree: SysVirtualAlloc failed at iteration %zu\n", i);
            std::abort();
        }
        if (!Common::SysVirtualFree(addr)) {
            std::fprintf(stderr, "BenchmarkAllocFree: SysVirtualFree failed at iteration %zu\n", i);
            std::abort();
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    const double avg_ns_per_op = static_cast<double>(elapsed) / static_cast<double>(iterations);
    const double ops_per_sec = (1e9 * static_cast<double>(iterations)) / static_cast<double>(elapsed);

    std::printf("[BENCHMARK] Alloc/Free (size = %zu bytes, iterations = %zu):\n", alloc_size, iterations);
    std::printf("  Total Time: %.3f ms\n", static_cast<double>(elapsed) / 1e6);
    std::printf("  Latency:    %.1f ns/op\n", avg_ns_per_op);
    std::printf("  Throughput: %.2f ops/sec\n\n", ops_per_sec);
}

// ---------------------------------------------------------------------------
// Benchmark 2: Reserve / Commit / Decommit / Free Lifecycle
// ---------------------------------------------------------------------------

void BenchmarkReserveCommitLifecycle(size_t iterations, size_t reserve_size) {
    const auto start = Clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t addr = Common::SysVirtualReserve(0, reserve_size);
        if (addr == 0) {
            std::fprintf(stderr, "BenchmarkReserveCommitLifecycle: Reserve failed at iteration %zu\n", i);
            std::abort();
        }
        if (!Common::SysVirtualCommit(addr, reserve_size, Common::VirtualMemory::Mode::ReadWrite)) {
            std::fprintf(stderr, "BenchmarkReserveCommitLifecycle: Commit failed at iteration %zu\n", i);
            std::abort();
        }
        if (!Common::SysVirtualDecommit(addr, reserve_size)) {
            std::fprintf(stderr, "BenchmarkReserveCommitLifecycle: Decommit failed at iteration %zu\n", i);
            std::abort();
        }
        if (!Common::SysVirtualFree(addr)) {
            std::fprintf(stderr, "BenchmarkReserveCommitLifecycle: Free failed at iteration %zu\n", i);
            std::abort();
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    const double avg_ns_per_op = static_cast<double>(elapsed) / static_cast<double>(iterations);
    const double ops_per_sec = (1e9 * static_cast<double>(iterations)) / static_cast<double>(elapsed);

    std::printf("[BENCHMARK] Reserve/Commit/Decommit/Free Lifecycle (size = %zu bytes, iterations = %zu):\n",
                reserve_size, iterations);
    std::printf("  Total Time: %.3f ms\n", static_cast<double>(elapsed) / 1e6);
    std::printf("  Latency:    %.1f ns/lifecycle\n", avg_ns_per_op);
    std::printf("  Throughput: %.2f lifecycles/sec\n\n", ops_per_sec);
}

// ---------------------------------------------------------------------------
// Benchmark 3: Protect State Transition Latency
// ---------------------------------------------------------------------------

void BenchmarkProtectTransitions(size_t iterations, size_t region_size) {
    uint64_t addr = Common::SysVirtualAlloc(0, region_size, Common::VirtualMemory::Mode::ReadWrite);
    if (addr == 0) {
        std::fprintf(stderr, "BenchmarkProtectTransitions: SysVirtualAlloc failed\n");
        std::abort();
    }

    const auto start = Clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        Common::VirtualMemory::Mode mode = (i % 2 == 0) ? Common::VirtualMemory::Mode::Read
                                                        : Common::VirtualMemory::Mode::ReadWrite;
        if (!Common::SysVirtualProtect(addr, region_size, mode)) {
            std::fprintf(stderr, "BenchmarkProtectTransitions: SysVirtualProtect failed at iteration %zu\n", i);
            std::abort();
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    Common::SysVirtualFree(addr);

    const double avg_ns_per_op = static_cast<double>(elapsed) / static_cast<double>(iterations);
    const double ops_per_sec = (1e9 * static_cast<double>(iterations)) / static_cast<double>(elapsed);

    std::printf("[BENCHMARK] Protect Transitions (size = %zu bytes, iterations = %zu):\n", region_size, iterations);
    std::printf("  Total Time: %.3f ms\n", static_cast<double>(elapsed) / 1e6);
    std::printf("  Latency:    %.1f ns/protect\n", avg_ns_per_op);
    std::printf("  Throughput: %.2f protects/sec\n\n", ops_per_sec);
}

// ---------------------------------------------------------------------------
// Benchmark 4: Aligned vs Unaligned Overhead
// ---------------------------------------------------------------------------

void BenchmarkAlignedOverhead(size_t iterations, size_t alloc_size, uint64_t alignment) {
    const auto start_unaligned = Clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t addr = Common::SysVirtualAlloc(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite);
        Common::SysVirtualFree(addr);
    }
    const auto elapsed_unaligned = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_unaligned).count();

    const auto start_aligned = Clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t addr = Common::SysVirtualAllocAligned(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite, alignment);
        Common::SysVirtualFree(addr);
    }
    const auto elapsed_aligned = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_aligned).count();

    const double unaligned_ns = static_cast<double>(elapsed_unaligned) / static_cast<double>(iterations);
    const double aligned_ns   = static_cast<double>(elapsed_aligned) / static_cast<double>(iterations);

    std::printf("[BENCHMARK] Aligned Allocation Overhead (size = %zu, alignment = 0x%" PRIx64 ", iterations = %zu):\n",
                alloc_size, alignment, iterations);
    std::printf("  Unaligned Latency: %.1f ns/op\n", unaligned_ns);
    std::printf("  Aligned Latency:   %.1f ns/op\n", aligned_ns);
    std::printf("  Alignment Overhead: %.1f ns (%.2fx)\n\n", aligned_ns - unaligned_ns, aligned_ns / unaligned_ns);
}

// ---------------------------------------------------------------------------
// Benchmark 5: Multi-threaded Contention (SysVirtualAlloc & SysVirtualFree)
// ---------------------------------------------------------------------------

void BenchmarkThreadContention(size_t num_threads, size_t ops_per_thread, size_t alloc_size) {
    const auto start = Clock::now();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([ops_per_thread, alloc_size]() {
            for (size_t i = 0; i < ops_per_thread; ++i) {
                uint64_t addr = Common::SysVirtualAlloc(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite);
                if (addr != 0) {
                    Common::SysVirtualFree(addr);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    const size_t total_ops = num_threads * ops_per_thread;
    const double avg_ns_per_op = static_cast<double>(elapsed) / static_cast<double>(total_ops);
    const double ops_per_sec = (1e9 * static_cast<double>(total_ops)) / static_cast<double>(elapsed);

    std::printf("[BENCHMARK] Multi-threaded Allocation Scaling (threads = %zu, ops/thread = %zu, alloc = %zu bytes):\n",
                num_threads, ops_per_thread, alloc_size);
    std::printf("  Total Time: %.3f ms\n", static_cast<double>(elapsed) / 1e6);
    std::printf("  Effective Latency: %.1f ns/op\n", avg_ns_per_op);
    std::printf("  Total Throughput:  %.2f ops/sec\n\n", ops_per_sec);
}

// ---------------------------------------------------------------------------
// Benchmark 6: Concurrent Read/Write Contention (SysVirtualProtect & Query vs Alloc)
// ---------------------------------------------------------------------------

void BenchmarkReadWriteContention(size_t num_threads, size_t ops_per_thread, size_t alloc_size) {
    uint64_t shared_region = Common::SysVirtualAlloc(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite);
    if (shared_region == 0) {
        std::fprintf(stderr, "BenchmarkReadWriteContention: SysVirtualAlloc failed\n");
        std::abort();
    }

    const auto start = Clock::now();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([ops_per_thread, alloc_size, shared_region, t]() {
            if (t % 2 == 0) {
                // Reader thread: query protection and mode transitions
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    Common::VirtualMemory::Mode old_mode;
                    Common::SysVirtualProtect(shared_region, alloc_size, Common::VirtualMemory::Mode::ReadWrite, &old_mode);
                }
            } else {
                // Writer thread: allocate & free local virtual regions
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    uint64_t addr = Common::SysVirtualAlloc(0, alloc_size, Common::VirtualMemory::Mode::ReadWrite);
                    if (addr != 0) {
                        Common::SysVirtualFree(addr);
                    }
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    Common::SysVirtualFree(shared_region);

    const size_t total_ops = num_threads * ops_per_thread;
    const double avg_ns_per_op = static_cast<double>(elapsed) / static_cast<double>(total_ops);
    const double ops_per_sec = (1e9 * static_cast<double>(total_ops)) / static_cast<double>(elapsed);

    std::printf("[BENCHMARK] Concurrent Read/Write Contention (threads = %zu, ops/thread = %zu, alloc = %zu bytes):\n",
                num_threads, ops_per_thread, alloc_size);
    std::printf("  Total Time: %.3f ms\n", static_cast<double>(elapsed) / 1e6);
    std::printf("  Effective Latency: %.1f ns/op\n", avg_ns_per_op);
    std::printf("  Total Throughput:  %.2f ops/sec\n\n", ops_per_sec);
}

} // namespace


int main() {
    std::printf("====================================================\n");
    std::printf(" KytyPS5 Virtual Memory Performance Benchmark Suite \n");
    std::printf("====================================================\n\n");

    Common::SysVirtualInit();

    BenchmarkAllocFree(5000, 65536);
    BenchmarkReserveCommitLifecycle(5000, 65536);
    BenchmarkProtectTransitions(10000, 65536);
    BenchmarkAlignedOverhead(2000, 65536, 0x10000); // 64 KB alignment

    std::printf("--- Multi-Threaded Allocation Scalability (1, 2, 4, 8, 16, 32 threads) ---\n\n");
    BenchmarkThreadContention(1, 2000, 65536);
    BenchmarkThreadContention(2, 2000, 65536);
    BenchmarkThreadContention(4, 2000, 65536);
    BenchmarkThreadContention(8, 2000, 65536);
    BenchmarkThreadContention(16, 2000, 65536);
    BenchmarkThreadContention(32, 2000, 65536);

    std::printf("--- Concurrent Read/Write Contention Scaling (1, 2, 4, 8, 16, 32 threads) ---\n\n");
    BenchmarkReadWriteContention(1, 2000, 65536);
    BenchmarkReadWriteContention(2, 2000, 65536);
    BenchmarkReadWriteContention(4, 2000, 65536);
    BenchmarkReadWriteContention(8, 2000, 65536);
    BenchmarkReadWriteContention(16, 2000, 65536);
    BenchmarkReadWriteContention(32, 2000, 65536);

    std::printf("VirtualMemoryBenchmarkTests: COMPLETED\n");
    return 0;
}

