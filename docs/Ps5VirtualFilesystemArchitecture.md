# PS5 Virtual Filesystem (VFS) Architecture

## Overview

The KytyPS5 Virtual Filesystem (VFS) provides a complete, thread-safe, cross-platform virtual file system layer emulating FreeBSD-derived PS5 filesystem layout, package container mounting, patch overlays, DLC management, save data isolation, high-speed cache/temporary storage, trophy management, path normalization, and package integrity verification.

---

## Architecture Diagram

```
+-------------------------------------------------------------------+
|                        Guest Application                          |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                      PathTranslator Engine                        |
|   (Normalizes /app0/.., handles case-insensitive fallback)        |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                    VfsPermissions Enforcement                     |
|  (ReadOnly: /app0, /patch, /addcont*, /system | ReadWrite: rest)  |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                    VirtualMountManager                            |
|     +-------------------------------------------------------+     |
|     | /patch Overlay (Highest Priority)                      |     |
|     | Fallback -> /app0 (Main Application)                  |     |
|     +-------------------------------------------------------+     |
|     | /addcont0..N (DLC Add-On Content Mounts)              |     |
|     | /savedata, /temp, /cache, /trophy, /dev, /system      |     |
|     +-------------------------------------------------------+     |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                 PkgParser & Integrity Verifier                    |
|        (Header magic 0x7F434E54, SHA-256 Digest Verification)     |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                        Host Filesystem                            |
+-------------------------------------------------------------------+
```

---

## Supported Mount Points

| Guest Mount | Mount Type | Default Flags | Description |
|-------------|------------|---------------|-------------|
| `/app0` | `App0` | `ReadOnly` | Main application executable and asset image |
| `/patch` | `Patch` | `ReadOnly` | Application update patch overlay (highest resolution priority) |
| `/addcont0`..`N` | `DLC` | `ReadOnly` | Downloadable content and add-on content packages |
| `/savedata` | `SaveData` | `ReadWrite` | Encrypted user save data partition |
| `/temp` | `Temp` | `ReadWrite` | Transient scratch storage |
| `/cache` | `Cache` | `ReadWrite` | High-speed texture and shader cache storage |
| `/trophy` | `Trophy` | `ReadWrite` | User trophy and achievement database storage |
| `/dev` | `Dev` | `ReadOnly` | Virtual device nodes (`/dev/null`, `/dev/console`) |
| `/system` | `System` | `ReadOnly` | System firmware libraries and common system assets |

---

## Transparent Patch Overlay Resolution

When a guest process queries or opens a file under `/app0/path/to/asset.bin`:
1. The `PathTranslator` queries `VirtualMountManager`.
2. The `VirtualMountManager` checks if `/patch/path/to/asset.bin` exists on the host filesystem.
3. If the patch file exists, the VFS transparently resolves to `/patch/path/to/asset.bin`.
4. If no patch file exists, the VFS falls back to `/app0/path/to/asset.bin`.

---

## Package Parser & Integrity Verification

The `PkgParser` module parses binary PS5 `.pkg` containers:
- **Header Magic**: `0x7F434E54` (`\x7FCNT`) or `0x7F504B47` (`\x7FPKG`).
- **Content ID**: 36-character content identifier (e.g. `HP0700-PPSA01234_00-GAME000000000000`).
- **Integrity**: `VerifyIntegrity()` validates SHA-256 header and entry digests against calculated hashes to ensure package integrity before installation.
