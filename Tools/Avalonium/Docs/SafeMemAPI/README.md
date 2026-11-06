# SafeMemAPI: Fault-Tolerant Memory Access & Inline Hooking

Avalonium reads and writes arbitrary engine memory from a live, protected game process. Every
access therefore has to survive a wild pointer, a torn-down object or an unmapped page without
killing the game. SafeMemAPI provides two facilities used across the entire runtime:

| Facility | Source | Purpose |
| --- | --- | --- |
| **Safe memory access** | [`SafeMemory.h`](../../Runtime/utils/SafeMemory.h) / [`SafeMemory.c`](../../Runtime/utils/SafeMemory.c) | SEH-guarded reads/writes, a fault-free page-table oracle, bulk span readers, and an engine-allocator call helper |
| **Inline hooking** | [`InlineHook.h`](../../Runtime/utils/InlineHook.h) / [`InlineHook.cpp`](../../Runtime/utils/InlineHook.cpp) | A 14-byte absolute-jump hook that works where MinHook's 2 GB relay search fails |

> **Why fault-freedom matters here.** Every intentional access violation becomes an
> **exception round trip** that parks the faulting thread. Screening loops that probe tens of
> thousands of wild pointers (measured: the object-array header screen ~107 k faults, the
> console-manager sweep ~180 k faults) would otherwise cost seconds. Hence the region-map
> oracle: **never fault** when the question is merely "is this readable?". And a *handled*
> fault is still a first-chance event to an attached debugger, the crash watchdog runs as one
> and pays a full capture per event, so even the map-covered read paths answer through the
> kernel probe instead of an SEH copy (see [REFERENCE.md §1.7](REFERENCE.md)).

---

## 1. The two questions

| Question | Function | Cost | Faults? |
| --- | --- | --- | --- |
| "Is this range mapped and readable?" | `SafeMem_IsMapped(addr, size)` | binary search over the committed-region map (if built), + one authoritative `VirtualQuery` (<1 µs) on a miss | never |
| "…same question, with a canonicality gate on the last byte" | `SafeMem_VerifyPointer(addr, size)` | identical to `IsMapped` | never |
| "What's in it?" | `SafeMem_Read` / `ReadPtr` / `ReadU32` / `ReadI32` / `ReadSpan` | L1-verified page → guarded `memcpy`; otherwise one `NtReadVirtualMemory` (kernel transition, map-covered or not) | only a guarded `memcpy` on a page that died between verification and the copy (caught) |

`SafeMem_IsReadable` is **the same function as `SafeMem_IsMapped`**, the header declares both
because callers read better with the intent named, and both answer from one region-map lookup or
authoritative `VirtualQuery`. Use `IsMapped`/`VerifyPointer` to screen pointers in bulk, and the `Read*`
family when you actually need the value. For anything larger than a few pages prefer **`SafeMem_ReadSpan`**:
it asks the map once for the whole span and reads the block through a single map-proven
`memcpy` or one kernel read, where the general `SafeMem_Read` path still pays the L1-page
bookkeeping.

---

## 2. Caches, and the fast-mode shim

Two caches exist, and they are not layered the way the names suggest:

| Tier | What it caches | Who uses it |
| --- | --- | --- |
| **L1** (`g_L1Cache`) | one 4 KB **page** per slot, direct-mapped, 256 slots, thread-local (~2 ns hit) | `SafeMem_ReadPtr`, `SafeMem_Read` (single-page fast path), `SafeMem_PrefetchRegion` |
| **L2** (`g_RegionMap`) | committed readable regions in the process, built by a single `VirtualQuery` walk into a dynamically grown array (capacity starts at 2048, doubles; the previous map is retained for lock-free readers); consulted when present without blocking startup on 128 TB address space scans | `IsMapped`, `IsReadable`, `VerifyPointer`, `SafeMem_Read` (multi-page), `SafeMem_ReadSpan` |

`SafeMem_ClearCache()` clears the calling thread's 256 page slots;
`SafeMem_PrefetchRegion(addr, size)` warms them with `1` for every page of the range.

```c
SafeMem_SetFastMode(1);   // enter (depth-counted, TLS)
SafeMem_SetFastMode(0);   // leave
```

> **Fast mode builds the map; it does not shorten the answer.** Entering `FastMemScope`
> (`depth == 1`) ensures the L2 committed-region map is built on-demand in a single pass, which is
> what makes the in-memory binary search the first stop for every `IsMapped`. A miss is still
> settled with one authoritative `VirtualQuery`: the map is a snapshot, and callers read a negative
> as a fact, a hook dispatch drops an out-parameter destination it was told is unreadable, a
> `FindAllOf` pass skips the object, so "not in the snapshot" must never be answered as
> "unreadable" (see [REFERENCE.md §1.3](REFERENCE.md)).

---

## 3. Function summary

| Function | Returns | Notes |
| --- | --- | --- |
| `SafeMem_IsMapped(addr, size)` | `int` | region-map binary search, authoritative single-allocation `VirtualQuery` fallback (<1 µs) |
| `SafeMem_IsReadable(addr, size)` | `int` | identical to `IsMapped` |
| `SafeMem_VerifyPointer(addr, size)` | `int` | `IsMapped` plus the canonical gate on the last byte |
| `SafeMem_IsProbeActive()` | `int` | `1` while inside a guarded probe (TLS) |
| `SafeMem_IsKernelProbeAvailable()` | `int` | `1` when `NtReadVirtualMemory` resolved |
| `SafeMem_SetFastMode(fast)` | N/A | depth-counted TLS toggle; at depth 1 it builds the L2 region map on demand |
| `SafeMem_PrefetchRegion(addr, size)` | N/A | warms the page cache |
| `SafeMem_ClearCache()` | N/A | clears the TLS page cache |
| `SafeMem_Read(addr, out, size)` | `int` | single page: L1-verified → guarded `memcpy`, otherwise one `NtReadRaw` whose verdict is cached; multi-page: all L1-verified → `memcpy`, otherwise one `NtReadRaw` for the span |
| `SafeMem_ReadSpan(addr, out, size)` | `size_t` | copies as much of the span as the page tables allow, reports the count |
| `SafeMem_ReadSpanExact(addr, out, size)` | `size_t` | `size` or `0`, all of it or nothing |
| `SafeMem_Write(dst, src, size)` | `int` | guarded write; retries through `VirtualProtect(PAGE_EXECUTE_READWRITE)`, then `NtWriteVirtualMemory` |
| `SafeMem_ReadPtr(addr)` | `void*` | L1-verified → guarded pointer read; otherwise one `NtReadRaw` whose verdict is cached; `NULL` on failure |
| `SafeMem_ReadU8/U16/U32/U64/I32/I64/Float/Double(addr, ok)` | value | `*ok` reports success; `ok` may be `NULL` |
| `SafeMem_ReadString(addr, out, maxLen)` | `size_t` | one guarded read **per character**, NUL-terminated |
| `SafeMem_ReadStringFast(addr, out, maxLen)` | `size_t` | 512-byte span reads, stops at the first NUL or gap |
| `SafeMem_Execute(fn, ctx)` | `int` | calls `int(*)(void*)` under SEH; returns `1` on fault |
| `SafeMem_Compare(addr, expected, size)` | `int` | guarded `memcmp`; `-1` on fault |
| `SafeMem_CallKeyFunction(fn, out)` | `int` | calls `void(*)(unsigned char*)` |
| `SafeMem_CallMallocVftable(instance, size, alignment, slot)` | `void*` | calls `FMalloc::Malloc` through the vtable, verifies the block is writable; the block is yours (never freed, first byte left as `0xA5`) |

C++ inline helpers: `SafeMem_IsCanonical`, `IsReadable`, `IsMapped`, `FastMemScope`.

**Bulk reads:** multi-page scan paths use `SafeMem_ReadSpanExact`, AES key chunk scans,
FName block reads, 256 KB executable-code scans, usmap bytecode capture, IPC region
scans, DataTable row compaction and `SCANINT`. Small probes and single-object reads stay
on `SafeMem_Read`, where the span reader has nothing to save. `SafeMem_ReadSpan` is
reached through `SafeMem_ReadSpanExact`.

`SafeMem_ReadStringFast`, `SafeMem_VerifyPointer` and `SafeMem_IsKernelProbeAvailable`
have no performance-relevant caller: the runtime has no per-character string reader,
`VerifyPointer` only adds the canonical gate to `IsMapped`, and kernel-probe availability
is diagnostic.

---

## 4. Canonical-address gate

Every entry point first rejects non-canonical user-space addresses:

```cpp
0x10000 <= addr <= 0x00007FFFFFFEFFFF
```

This rejects null/low pages, kernel addresses and 32-bit-style garbage before any probing.
Note the range test is inclusive on both ends and applies to **both** the start and the last
byte of the range (so `addr + size - 1` must also be canonical).

---

## 5. Where it is used

* **Engine discovery**: `PatternScanner`, offset probes, object-array walks, FName resolution.
* **The object index**: `FindLiveObjectsByClass`, `IsObjectLiveBestEffort`.
* **Lua memory API**: `Memory.ReadInt32`, `ReadFString`, `IsValidPtr`, …
* **The AES key finder**: 64 KB chunk scans through `SafeMem_ReadSpanExact`.
* **Bulk discovery scans**: 256 KB executable-code chunks, FName blocks, usmap bytecode
  capture, IPC region scans and DataTable row compaction use `SafeMem_ReadSpanExact`.
* **The console manager**: `ConsoleDumpers`/`ConsoleHelpers` screen thousands of candidate
  pointers with `SafeMem_IsReadable`.
* **The overlay**: live property reads in the inspector/Live Editor.

See [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md) and
[LuaAPI/GLOBALS.md](../LuaAPI/GLOBALS.md).

---

## 6. Inline hooking in one paragraph

`Avalonium::InlineHook` patches the target's first bytes with an **absolute** jump
(`FF 25 00 00 00 00 <detour>`) and builds a trampoline holding the displaced instructions plus
the same absolute jump back. Because both halves are absolute, no relay page near the target
is needed, which is exactly why it exists: MinHook patches with a 5-byte **relative** jump
whose relay must be allocated within ±2 GB, and on real titles that search fails
(`MH_ERROR_MEMORY_ALLOC`) even when the window is not full. The one thing a copied instruction
still needs is its RIP-relative operand, so the hook **carries** it on a spare register
(`push r; mov r, <address>; <instruction, addressing through r>; pop r`) instead of searching
for a place within reach.

Full details: [REFERENCE.md](REFERENCE.md).

---

## 7. Source map

| File | Contents |
| --- | --- |
| `SafeMemory.h` | C API, C++ inline helpers, `FastMemScope` |
| `SafeMemory.c` | L1 page cache, dynamically grown region map, NT probes, bulk readers, all guarded operations |
| `InlineHook.h` | `InlineHook` class, `kInlineHookPatchBytes = 14` |
| `InlineHook.cpp` | installer, trampoline builder, RIP-carry rewriter, summary/placement notes |

---

## Cross-references

* [REFERENCE.md](REFERENCE.md): every function, the region-map algorithm, the hook internals.
* [LuaAPI/GLOBALS.md](../LuaAPI/GLOBALS.md#10-memory-table-and-ue-snake_case-equivalents): Lua
  memory bindings built on this API.
* [LuaAPI/HOOKS.md](../LuaAPI/HOOKS.md): the ProcessEvent/script patches that use `InlineHook`.
* [CryptoAPI/REFERENCE.md](../CryptoAPI/REFERENCE.md): bulk scans through `SafeMem_Read`.
