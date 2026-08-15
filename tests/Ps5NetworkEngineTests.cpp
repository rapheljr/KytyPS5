// Ps5NetworkEngineTests.cpp
//
// Unit tests for PS5 Virtual Network Socket & SceNetCtl Engine.

#include "compat/ps5NetworkEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>

#define ASSERT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			::printf("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
			::exit(1); \
		} \
	} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static void Test_Ps5Network_LifecycleAndCtl() {
	::printf("[TEST] Ps5Network_LifecycleAndCtl\n");
	Compat::Ps5NetworkEngine engine;
	ASSERT_FALSE(engine.IsInitialized());

	ASSERT_TRUE(engine.Initialize());
	ASSERT_TRUE(engine.IsInitialized());
	ASSERT_EQ(engine.GetActiveSocketCount(), 0U);

	Compat::SceNetCtlInfo info;
	ASSERT_TRUE(engine.GetCtlInfo(&info));
	ASSERT_EQ(info.state, Compat::SceNetCtlState::IpObtained);
	ASSERT_EQ(info.ip_address, "192.168.1.105");
	ASSERT_EQ(info.mtu, 1500U);

	engine.SetCtlState(Compat::SceNetCtlState::Disconnected);
	ASSERT_TRUE(engine.GetCtlInfo(&info));
	ASSERT_EQ(info.state, Compat::SceNetCtlState::Disconnected);

	engine.Shutdown();
	ASSERT_FALSE(engine.IsInitialized());
	::printf("  [ OK ] Ps5Network_LifecycleAndCtl\n");
}

static void Test_Ps5Network_SocketOperations() {
	::printf("[TEST] Ps5Network_SocketOperations\n");
	Compat::Ps5NetworkEngine engine;
	ASSERT_TRUE(engine.Initialize());

	// Create TCP Stream socket
	int32_t sock = engine.Socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_TRUE(sock >= 1000);
	ASSERT_EQ(engine.GetActiveSocketCount(), 1U);

	// Bind to local loopback port 0 (ephemeral)
	Compat::SceNetSockAddrIn bind_addr{};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = 0;
	bind_addr.sin_addr = ::inet_addr("127.0.0.1");

	int32_t bind_res = engine.Bind(sock, &bind_addr, sizeof(bind_addr));
	ASSERT_EQ(bind_res, 0);

	// Listen
	int32_t listen_res = engine.Listen(sock, 5);
	ASSERT_EQ(listen_res, 0);

	// Close socket
	ASSERT_EQ(engine.Close(sock), 0);
	ASSERT_EQ(engine.GetActiveSocketCount(), 0U);

	::printf("  [ OK ] Ps5Network_SocketOperations\n");
}

int main() {
	::printf("================================================================================\n");
	::printf("  KytyPS5 — PS5 Virtual Network Engine Test Suite\n");
	::printf("================================================================================\n");

	Test_Ps5Network_LifecycleAndCtl();
	Test_Ps5Network_SocketOperations();

	::printf("================================================================================\n");
	::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	::printf("================================================================================\n");
	return 0;
}
