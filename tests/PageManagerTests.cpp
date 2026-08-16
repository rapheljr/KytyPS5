#include "graphics/host_gpu/pageManager.h"
#include "common/virtualMemory.h"
#include "kernel/memory.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

using Libs::Graphics::PageManager;
using Libs::Graphics::RegionBits;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "PageManagerTests: failed: %s\n", text);
		std::abort();
	}
}

void TestPageManagerLifecycle() {
	PageManager manager;
	const auto  page_size = manager.GetPageSize();
	Check(page_size >= 4096, "invalid host/guest page size");

	const uint64_t size  = page_size * 4;
	const uint64_t vaddr = Libs::LibKernel::Memory::AllocateProgramMemory(
	    0, size, Common::VirtualMemory::Mode::ReadWrite, "page_manager_test_1");
	Check(vaddr != 0, "AllocateProgramMemory failed");

	manager.OnGpuMap(vaddr, size);

	// Track pages
	manager.UpdatePageWatchers<true>(vaddr, page_size * 2);

	// Untrack pages
	manager.UpdatePageWatchers<false>(vaddr, page_size * 2);

	manager.OnGpuUnmap(vaddr, size);
	Libs::LibKernel::Memory::FreeGuestMemory(vaddr, size);
}

void TestRegionWatchers() {
	PageManager manager;
	const auto  page_size = manager.GetPageSize();

	const uint64_t size      = page_size * 64;
	const uint64_t base_addr = Libs::LibKernel::Memory::AllocateProgramMemory(
	    0, size, Common::VirtualMemory::Mode::ReadWrite, "page_manager_test_2");
	Check(base_addr != 0, "AllocateProgramMemory failed");

	manager.OnGpuMap(base_addr, size);

	RegionBits mask;
	mask.Set(0);
	mask.Set(1);
	mask.Set(2);

	manager.UpdatePageWatchersForRegion<true, false>(base_addr, mask);
	manager.UpdatePageWatchersForRegion<false, false>(base_addr, mask);

	manager.UpdatePageWatchersForRegion<true, true>(base_addr, mask);
	manager.UpdatePageWatchersForRegion<false, true>(base_addr, mask);

	manager.OnGpuUnmap(base_addr, size);
	Libs::LibKernel::Memory::FreeGuestMemory(base_addr, size);
}

void TestMultipleMappings() {
	PageManager manager;
	const auto  page_size = manager.GetPageSize();

	const uint64_t size = page_size * 8;
	const uint64_t base1 = Libs::LibKernel::Memory::AllocateProgramMemory(
	    0, size, Common::VirtualMemory::Mode::ReadWrite, "page_manager_test_3a");
	const uint64_t base2 = Libs::LibKernel::Memory::AllocateProgramMemory(
	    0, size, Common::VirtualMemory::Mode::ReadWrite, "page_manager_test_3b");
	Check(base1 != 0 && base2 != 0, "AllocateProgramMemory failed");

	manager.OnGpuMap(base1, size);
	manager.OnGpuMap(base2, size);

	manager.UpdatePageWatchers<true>(base1, page_size * 4);
	manager.UpdatePageWatchers<true>(base2, page_size * 4);

	manager.UpdatePageWatchers<false>(base1, page_size * 4);
	manager.UpdatePageWatchers<false>(base2, page_size * 4);

	manager.OnGpuUnmap(base1, size);
	manager.OnGpuUnmap(base2, size);

	Libs::LibKernel::Memory::FreeGuestMemory(base1, size);
	Libs::LibKernel::Memory::FreeGuestMemory(base2, size);
}

} // namespace

int main() {
	Common::VirtualMemory::Init();
	Libs::LibKernel::Memory::Initialize();

	TestPageManagerLifecycle();
	TestRegionWatchers();
	TestMultipleMappings();

	std::puts("PageManagerTests: all cases passed");
	return 0;
}
