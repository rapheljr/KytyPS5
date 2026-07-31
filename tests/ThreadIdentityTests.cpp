#include "common/threads.h"
#include "common/config.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <thread>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ThreadIdentityTests: failed: %s\n", text);
		std::abort();
	}
}

void TestMainThreadIdentification() {
	Check(Common::Thread::IsMainThread(), "IsMainThread must return true on main thread");
}

void TestUniqueThreadIds() {
	constexpr int THREAD_COUNT = 16;
	std::vector<std::thread> threads;
	std::atomic<int> ids[THREAD_COUNT] {};

	for (int i = 0; i < THREAD_COUNT; i++) {
		threads.emplace_back([i, &ids] {
			ids[i].store(Common::Thread::GetThreadIdUnique());
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	std::set<int> unique_set;
	for (int i = 0; i < THREAD_COUNT; i++) {
		const int id = ids[i].load();
		Check(id > 0, "ThreadIdUnique must be positive");
		Check(unique_set.insert(id).second, "ThreadIdUnique must be distinct across threads");
	}
}

} // namespace

int main() {
	TestMainThreadIdentification();
	TestUniqueThreadIds();

	std::printf("ThreadIdentityTests: PASSED\n");
	return 0;
}
