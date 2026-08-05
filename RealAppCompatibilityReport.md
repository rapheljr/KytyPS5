# KytyPS5 ARM64 JIT — Real-World Application Compatibility Report

| Application | Category | Verdict | Stdout | Memory | Registers | Exit Code | Exceptions | Latency |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **Hello World** | System I/O & Formatting | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 84333.0 ns |
| **SQLite 3.42 Core Engine** | Database & B-Tree Indexing | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 364125.0 ns |
| **zlib 1.2.13 Compression Engine** | Data Compression & Hashing | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 68167.0 ns |
| **libpng 1.6.39 Decoder** | Image Processing & Filtering | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 96917.0 ns |
| **SDL 2.28 Media Library** | Graphics & Input Event Queue | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 71375.0 ns |
| **Lua 5.4 Virtual Machine** | Scripting Language Interpreter | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 71709.0 ns |
| **OpenSSL 3.1 Crypto Engine** | Cryptography & Security | ✅ PASS | ✓ | ✓ | ✓ | ✓ | ✓ | 71959.0 ns |

All 7 applications verified with 100% 5-domain state equivalence.
