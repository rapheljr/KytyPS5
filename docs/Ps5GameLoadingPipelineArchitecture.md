# PS5 Game Loading Pipeline Architecture

## Overview

The KytyPS5 Game Loading Pipeline orchestrates end-to-end binary loading, container parsing, update/patch overlays, add-on DLC mounting, dynamic relocation, symbol NID binding, module dependency graph resolution, and guest process kernel startup.

---

## Architecture Diagram

```
+-------------------------------------------------------------------+
|                        Game Input Target                          |
|             (.pkg Archive / Directory / ELF / SELF)                |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                      VirtualMountManager                          |
|   - Mount /app0 root filesystem                                   |
|   - Mount /patch update overlay (Priority 10 over /app0)          |
|   - Mount /addcont0..N DLC content packages                       |
|   - Mount /savedata, /temp, /cache, /trophy                       |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|               Binary Parser (SelfParser & Elf64)                  |
|   - Detect & decrypt PS5 Signed ELF (0x4F534C46 "OSLF")           |
|   - Parse PT_LOAD segments & PT_OS_PROCPARAM                       |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                 RuntimeLinker & SymbolResolver                    |
|   - Map guest memory segments                                     |
|   - Resolve NID symbol imports/exports (libKernel, system libs)   |
|   - Execute Relocations (R_X86_64 & R_AARCH64 relative/jump)      |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                     ModuleDependencyGraph                         |
|   - Directed Acyclic Graph (DAG) for DT_NEEDED modules            |
|   - Topological sort for init_array / module_start execution      |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                     Kernel Process Startup                        |
|   - ProcessManager: Instantiate guest process PCB                |
|   - KernelScheduler: Spawn primary thread at entry point          |
+-------------------------------------------------------------------+
```

---

## Key Subsystems

1. **SelfParser (`selfParser.h` / `.cpp`)**:
   - Parses PS5 Signed ELF (`0x4F534C46` magic header), metadata block, and segment tables.
   - Extracts raw ELF executable payload.

2. **ModuleDependencyGraph (`moduleDependencyGraph.h` / `.cpp`)**:
   - Directed acyclic graph representation of loaded modules/libraries.
   - Implements Tarjan's / Kahn's topological sort algorithm to determine exact module initialization order (`init_array`, `module_start`).
   - Detects circular dependencies and produces dependency topology reports.

3. **GameLoadingPipeline (`gameLoadingPipeline.h` / `.cpp`)**:
   - Master orchestrator for PKG container extraction, update overlays (`/patch` over `/app0`), DLC add-on content (`/addcont0..N`), ELF/SELF segment mapping, relocation execution, symbol NID binding, and kernel process launch.
