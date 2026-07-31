#include "common/config.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if KYTY_PLATFORM == KYTY_PLATFORM_MACOS || defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#endif

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "MachApiTests: failed: %s\n", text);
		std::abort();
	}
}

void TestMachThreadPortConsistency() {
#if KYTY_PLATFORM == KYTY_PLATFORM_MACOS || defined(__APPLE__)
	const mach_port_t self1 = pthread_mach_thread_np(pthread_self());
	const mach_port_t self2 = pthread_mach_thread_np(pthread_self());
	Check(self1 != 0, "pthread_mach_thread_np must return valid non-zero port");
	Check(self1 == self2, "pthread_mach_thread_np port must be consistent");
#endif
}

void TestMachVmRegionQueryPortDeallocation() {
#if KYTY_PLATFORM == KYTY_PLATFORM_MACOS || defined(__APPLE__)
	int sample_var = 42;
	auto query_addr = reinterpret_cast<mach_vm_address_t>(&sample_var);

	for (int i = 0; i < 1000; i++) {
		mach_vm_address_t region_addr = query_addr;
		mach_vm_size_t    region_size = 0;
		vm_region_basic_info_data_64_t info {};
		mach_msg_type_number_t count       = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t            object_name = MACH_PORT_NULL;

		kern_return_t kr =
		    mach_vm_region(mach_task_self(), &region_addr, &region_size, VM_REGION_BASIC_INFO_64,
		                   reinterpret_cast<vm_region_info_t>(&info), &count, &object_name);
		Check(kr == KERN_SUCCESS, "mach_vm_region must succeed on valid address");
		Check(region_addr <= query_addr, "mach_vm_region address alignment check");
		Check(region_addr + region_size > query_addr, "mach_vm_region size check");

		if (MACH_PORT_VALID(object_name)) {
			mach_port_deallocate(mach_task_self(), object_name);
		}
	}
#endif
}

} // namespace

int main() {
	TestMachThreadPortConsistency();
	TestMachVmRegionQueryPortDeallocation();

	std::printf("MachApiTests: PASSED\n");
	return 0;
}
