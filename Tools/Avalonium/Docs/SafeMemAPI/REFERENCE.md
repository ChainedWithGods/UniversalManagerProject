# SafeMemAPI: Reference

Complete reference for [`SafeMemory.h`](../../Runtime/utils/SafeMemory.h) /
[`SafeMemory.c`](../../Runtime/utils/SafeMemory.c) and
[`InlineHook.h`](../../Runtime/utils/InlineHook.h) / [`InlineHook.cpp`](../../Runtime/utils/InlineHook.cpp).

---

# Part 1: Safe memory access

## 1.1 State

Three pieces of mutable state, plus the resolved NT entry points.

```c
/* NT kernel probes, resolved once (SafeMem_InitNtApis) */
static pfnNtReadVirtualMemory  g_pfnNtReadVirtualMemory;   /* NtReadVirtualMemory  */
static pfnNtWriteVirtualMemory g_pfnNtWriteVirtualMemory;  /* NtWriteVirtualMemory */
static pfnNtQueryVirtualMemory g_pfnNtQueryVirtualMemory;  /* resolved, not used   */
static volatile LONG g_NtApisInitialized;

/* Probe flag and fast-mode depth (thread-local) */
__declspec(thread) int g_SafeMemProbeActive    = 0;   /* exported: read by SafeMem_IsProbeActive */
static __declspec(thread) int g_SafeMemFastModeDepth = 0;

/* Tier 1 - per-thread page cache */
typedef struct {
  uintptr_t PageBase;   /* 4KB-aligned page address */
  uint8_t   Status;     /* 0 = unreadable, 1 = readable, 2 = writable (declared, never written) */
} SafeMemL1Entry;

static __declspec(thread) SafeMemL1Entry g_L1Cache[SAFMEM_L1_CACHE_SIZE];

#define SAFEMEM_L1_CACHE_SIZE  256
#define SAFEMEM_L1_CACHE_MASK  (SAFMEM_L1_CACHE_SIZE - 1)   /* 255 */
#define SAFEMEM_PAGE_SHIFT     12
#define SAFEMEM_PAGE_SIZE      4096ULL
#define SAFEMEM_PAGE_MASK      (~(SAFEMEM_PAGE_SIZE - 1))
```

The L1 cache is **direct-mapped per 4 KB page**: the slot is `(pageBase >> 12) & 255`, so one
slot covers 1 MB of address space and a page more than 1 MB away evicts it. There is no
region-extent cache and no `Readable` tri-state union range, a slot records exactly one page.

**Which functions consult it:** `SafeMem_ReadPtr`, the single-page and all-cached branches of
`SafeMem_Read`, and `SafeMem_PrefetchRegion` (which fills it with `1` for every page of a span).
**Which do not:** `SafeMem_IsReadable`, `SafeMem_IsMapped`, `SafeMem_VerifyPointer`,
`SafeMem_ReadSpan`: all four go straight to the region map.

```c
/* Tier 2 - process-wide committed-region map (heap, exact size) */
typedef struct { uintptr_t Start; uintptr_t End; } SafeMemMapRegion;

static SafeMemMapRegion *volatile g_RegionMap;      /* published last */
static size_t                    g_RegionMapCapacity;
static volatile LONG             g_RegionMapCount;     /* published after the fill */
static volatile LONG             g_RegionMapBuiltTick;
static SRWLOCK                   g_RegionMapLock = SRWLOCK_INIT;

#define SAFEMEM_REGION_MAP_MIN_REBUILD_MS 30000
```

There is **no capacity ceiling and no "unusable" fallback state**. The array is allocated to the
exact number of committed readable regions the address space reports, so nothing is ever
truncated; if the count grows past the current allocation a larger one is published and the old
one is left allocated forever (readers search it without holding the lock, so an array must never
be freed or rewritten under them).

## 1.2 `SafeMem_IsProbeActive()` / `SafeMem_IsKernelProbeAvailable()`

| Function | Returns |
| --- | --- |
| `SafeMem_IsProbeActive()` | the calling thread's `g_SafeMemProbeActive` |
| `SafeMem_IsKernelProbeAvailable()` | `1` when `NtReadVirtualMemory` resolved successfully |

`g_SafeMemProbeActive` is set to `1` immediately before a guarded access and cleared in both the
normal and the `__except` path, by: `SafeMem_Read`, `SafeMem_ReadSpan`, `SafeMem_ReadPtr`,
`SafeMem_Write`, `SafeMem_Compare`, `SafeMem_Execute`. It marks a guarded access as an
**intentional probe**, so a fault handler that observes the flag can tell it from a real crash.
The typed readers (`ReadU8`…`ReadDouble`), `ReadString`, `ReadStringFast`
and `ReadSpanExact` do **not** touch the flag themselves, they delegate to `SafeMem_Read` /
`SafeMem_ReadSpan`, which do. `SafeMem_CallKeyFunction` and `SafeMem_CallMallocVftable` never
raise the flag at all, even though both execute foreign code under `__try`.

`SafeMem_IsKernelProbeAvailable` calls `SafeMem_InitNtApis`, which resolves `NtReadVirtualMemory`,
`NtWriteVirtualMemory` and `NtQueryVirtualMemory` from `ntdll.dll` once behind an
`InterlockedCompareExchange` guard.

## 1.3 `SafeMem_SetFastMode` / `SafeMem_ClearCache` / `SafeMem_PrefetchRegion`

| Function | Behavior |
| --- | --- |
| `SafeMem_SetFastMode(1)` | `++g_SafeMemFastModeDepth` (TLS) |
| `SafeMem_SetFastMode(0)` | decrements if `> 0`; extra releases are ignored (never underflows) |
| `SafeMem_ClearCache()` | `memset` the 256-entry TLS page cache |
| `SafeMem_PrefetchRegion(addr, size)` | `SafeMem_L1Insert(page, 1)` for every page in `[addr, addr+size)`; returns immediately if the range is non-canonical |

> **Fast mode does not shorten the query result.** Entering `FastMemScope` / `SafeMem_SetFastMode(1)`
> (`depth == 1`) ensures the L2 committed-region map is built on-demand in a single fast pass, but it does not
> change what `SafeMem_IsMapped` answers: a miss is always settled with one authoritative `VirtualQuery`.
> The map is a snapshot, memory committed after it was built (a thread stack that grows deeper, a fresh heap
> chunk) is absent from it, and callers read a negative as a fact: a hook dispatch drops the out-parameter
> destination it was told is unreadable (`LuaHookManager::ResolveOutParmAddress`), a `FindAllOf` pass skips the
> object. Giving "not in the snapshot" as "unreadable" was a real, intermittent defect (dropped out-parameter
> values on freshly allocated memory), so the query never takes that shortcut. Call sites keep entering fast
> mode for the on-demand map build and for call-site compatibility (see `SafeMemory.h`).

## 1.4 `SafeMem_IsMapped(addr, size)`

Fault-free mapped-memory oracle. Returns `1` when `[addr, addr+size)` lies entirely inside
committed, readable regions.

1. Reject `NULL`, `size == 0`, non-canonical `start`/`end`, or `end < start` (overflow).
2. `SafeMem_RegionMapCovers(start, end)`: binary-search the map for the **last region starting at
   or before `start`**, then walk forward accumulating `covered` while `map[i].Start <= covered`,
   until `covered > end` (→ mapped) or a gap appears. If the region map is unbuilt (`g_RegionMapCount == 0`),
   it returns `0` immediately rather than halting startup to scan the 128 TB virtual address space.
3. On a miss, do an **authoritative** `SafeMem_RegionAt(start)` check (a single `VirtualQuery`, <1 µs):
   the range is mapped when `end < regionEnd`. This verifies the OS page tables directly for that single
   allocation without blocking execution.

`SafeMem_RegionAt(addr, &start, &end)` treats a region as readable when `State == MEM_COMMIT`, the
low protection byte is neither `PAGE_NOACCESS` nor `PAGE_EXECUTE`, and `PAGE_GUARD` is not set.
(`PAGE_EXECUTE` alone is excluded; `PAGE_EXECUTE_READ` / `READWRITE` / `WRITECOPY` are accepted.)

Map construction (`SafeMem_RebuildRegionMap`) is available for bulk whole-binary analysis scans. It walks
`VirtualQuery` from `0x10000` to `0x00007FFFFFFEFFFF` in **one pass**, appending committed readable regions
to a dynamically grown array (capacity starts at 2048 and doubles; the previous map is intentionally not
freed, because readers search it without holding the lock). Rebuilds are rate-limited to one per 30 s and
are never triggered by single-pointer reads; they happen when `SafeMem_SetFastMode(1)` enters at depth 1
with no map yet, or as a stale-map repair.

## 1.5 `SafeMem_IsReadable(addr, size)` and `SafeMem_VerifyPointer(addr, size)`

```c
int SafeMem_IsReadable(const void* addr, size_t size) { return SafeMem_IsMapped(addr, size); }
```

That is the whole implementation. `IsReadable` and `IsMapped` are the **same query**, a
region-map binary search with an authoritative `VirtualQuery` fallback, and neither consults the
L1 page cache nor fast mode. There is no per-page walk and no 64 KB-extent cache any more; both
cost a binary search over the region array plus, on a miss, one `VirtualQuery`.

`SafeMem_VerifyPointer(addr, size)` is the same answer with the canonical gate applied to both the
first and the last byte of the range (`a + size - 1`) before the lookup, and `0` for null /
zero-size.

## 1.6 Bulk span readers

```c
size_t SafeMem_ReadSpan(const void* addr, void* out, size_t size);
size_t SafeMem_ReadSpanExact(const void* addr, void* out, size_t size);
```

`SafeMem_ReadSpan` is the read path for bulk data, a 256 KB code block, a 1.5 MB object-array
chunk, the 332 MB `.text` of a shipping build. It asks the region map **once for the whole span**
rather than once per page:

1. Canonical gate on `start`, `end = start + size - 1`, and `end < start`.
2. **Map-proven fast path:** `SafeMem_RegionMapCovers` → a single `memcpy` under `__try`, return
   `size`. A fault here means a region vanished between the lookup and the copy, and falls through.
3. **Conservative path:** `VirtualQuery` the span and `memcpy` each committed readable run in turn,
   stopping at the first genuine gap or fault. `VirtualQuery` is the authority, so a mapping the
   cached map has not seen yet is still read without faulting.
4. **Last resort:** if not one byte was copied, a single `SafeMem_NtReadRaw` (kernel probe, zero
   user-mode faults) for the whole size.

Returns `size` when the entire span was readable, a smaller count when the mapping ended early
(the prefix in `out` is valid), and `0` when the first byte was unreadable.
`SafeMem_ReadSpanExact` collapses that to `got == size ? size : 0`.

> **Neither span reader has a caller yet.** Every bulk read in the runtime still goes through
> `SafeMem_Read`. Its multi-page path first asks the region map for the whole span and answers
> through one kernel read; if the map does not cover it, it tries an all-L1-cached `memcpy`, and
> otherwise performs one `NtReadRaw` for the whole range (caching every page on success). There
> is no per-page read sweep. Porting the 256 KB chunk reads in `FunctionFinder` and the
> object-array walks in `WorldDiscovery` to `SafeMem_ReadSpan` is still the intended next step.

## 1.7 Guarded operations

All of the following apply the canonical gate to **both** the first and the last byte of the range
before touching memory.

> **Map-covered reads are answered by the kernel probe, never by an SEH copy.** The map proves a
> span *was* committed when it was built, not that its pages still are; a page released since
> answers a handled `__except` fault that an attached debugger, the crash watchdog runs as one,
> still observes as a first-chance event and pays a full capture for (thread suspension, stack
> walk, report write). `SafeMem_ReadPtr` and `SafeMem_Read` therefore route both map-covered paths
> (`IsMapped` hit, whole-span map proof) through `SafeMem_NtReadRaw`, the same zero-fault probe the
> fallback uses. Only a verified read enters L1 and a failed probe caches `0`, so a stale page
> costs one kernel transition per cache lifetime instead of a fault; the L1 fast path stays a plain
> guarded read because L1 only ever holds pages a probe has already verified.

| Function | Signature | Behavior |
| --- | --- | --- |
| `SafeMem_Read` | `int (const void* addr, void* out, size_t size)` | single-page fast path (99.9% of reads): L1 lookup (~2 ns) → `memcpy`; otherwise the read goes through `NtReadRaw`, map-covered or not, and its verdict enters L1. Multi-page: all pages L1-verified → `memcpy`; otherwise one `NtReadRaw` for the whole span. Returns `1`/`0` |
| `SafeMem_Write` | `int (void* dst, const void* src, size_t size)` | `memcpy`; on fault retries once through `VirtualProtect(PAGE_EXECUTE_READWRITE)` and restores the original protection (also on a second fault); if that fails, `NtWriteVirtualMemory` |
| `SafeMem_ReadPtr` | `void* (const void* addr)` | L1 page hit → direct read (`0` cached on fault); otherwise one `NtReadRaw`, map-covered or not, with its verdict entering L1; `NULL` on any failure. A cached **unreadable** page returns `NULL` without probing |
| `SafeMem_ReadU8/U16/U32/U64/I32/I64/Float/Double` | `T (const void* addr, int* ok)` | `SafeMem_Read` of the value into a local; `*ok` reports success, value is `0` on failure. `ok` may be `NULL` |
| `SafeMem_Compare` | `int (const void* addr, const void* expected, size_t size)` | `size <= 256`: `SafeMem_Read` into a 256-byte stack buffer then `memcmp`; larger: `memcmp` directly under `__try`. `-1` on fault or bad args (a genuine `memcmp` result of `-1` is therefore indistinguishable from a fault) |
| `SafeMem_ReadString` | `size_t (const void* addr, char* out, size_t maxLen)` | one `SafeMem_Read` **per character**, always NUL-terminated, returns the length excluding the terminator |
| `SafeMem_ReadStringFast` | `size_t (const void* addr, char* out, size_t maxLen)` | copies 512-byte spans via `SafeMem_ReadSpan`, stops at the first NUL or the first byte the mapping cannot deliver, always NUL-terminates |
| `SafeMem_Execute` | `int (SafeExecFunc func, void* context)` | calls `int(*)(void*)` under `__try`; returns `1` on fault and `1` for a null function, otherwise the function's own result |
| `SafeMem_CallKeyFunction` | `int (const void* fn_ptr, unsigned char* out_buf)` | calls `void(*)(unsigned char*)` under `__try`; `1` on success, `0` on fault/bad args. **Does not raise the probe flag** |

`SafeMem_ReadStringFast` exists because `SafeMem_ReadString` costs one guarded access per
character, which across the names discovery walks (FName entries, class names, property names) is
the difference between a handful of span copies and thousands of page-cache probes.

### `SafeMem_CallMallocVftable(instance, size, alignment, mallocSlotIndex)`

Calls `FMalloc::Malloc` through an engine allocator instance's vtable.

* Refuses when `instance` is null/non-canonical, `size == 0`, or `mallocSlotIndex < 0`, an
  unmeasured slot index is never guessed, since the wrong slot can be a destructor.
* Reads `vtable = *(void**)instance` with `SafeMem_ReadPtr`, then the slot with `SafeMem_ReadPtr`,
  and validates that the slot is canonical.
* Calls `void*(__fastcall*)(const void* self, size_t count, uint32_t alignment)`.
* **Verifies the answer:** the returned block must be canonical and *writable*, it stores `0xA5`
  into the first byte and reads it back. A slot that answered a string literal, a small integer or
  a read-only address is reported as `NULL` instead of being written through.

Two side effects the caller owns: the block is **never freed**, and on success its first byte is
still `0xA5`.

Layout note (documented in the header): FMalloc's primary polymorphic base is `FExec`, so on MSVC
x64 slots 0–1 are the destructor pair, slot 2 is `FExec::Exec`, and slot 3 is
`Malloc(Count, Alignment)`: but the *measured* index for the running build is what must be
passed; `TryMalloc`/`TryRealloc` only exist from 4.25+ and are never touched.

## 1.8 C++ helpers (C++ inline)

```cpp
SafeMem_IsCanonical(const void* addr)          // 0x10000 <= a <= 0x00007FFFFFFEFFFF
IsReadable(const void* addr, size_t = 8)       // == IsMapped, default size sizeof(void*)
IsMapped(const void* addr, size_t = 8)
struct FastMemScope { FastMemScope(); ~FastMemScope(); };  // RAII fast mode (see 1.3: inert)
```

## 1.9 Cost model

| Operation | Cost |
| --- | --- |
| `SafeMem_IsMapped` / `IsReadable` / `VerifyPointer`, hit | binary search over the region array |
| …, miss | binary search + one `VirtualQuery` (+ at most one rebuild per 30 s) |
| `SafeMem_ReadSpan`, map-proven | one `memcpy`, no probe |
| `SafeMem_ReadSpan`, map-proven but the copy faults | region-by-region `VirtualQuery` + `memcpy` per run |
| `SafeMem_ReadSpan`, nothing accounted for | one `NtReadVirtualMemory` (kernel transition, no user-mode fault) |
| `SafeMem_ReadPtr`, L1 hit | one TLS array lookup + one guarded load |
| `SafeMem_ReadPtr` / `SafeMem_Read`, no L1 verdict | one `NtReadVirtualMemory` (kernel transition, no user-mode fault); the verdict enters L1 |
| `SafeMem_Read`, single page, L1-verified | one guarded `memcpy` |
| `SafeMem_Read`, multi-page, all pages L1-verified | one guarded `memcpy` |
| `SafeMem_Read`, multi-page, map-proven or unaccounted | one `NtReadVirtualMemory` for the whole span |
| Any faulting access | a VEH/debugger round trip, this is what the oracle exists to avoid |

---

# Part 2: Inline hooking

*(verified against `InlineHook.h` / `InlineHook.cpp`)*

## 2.1 Shape

```
at the target         FF 25 00 00 00 00 <detour>            jmp qword ptr [rip+0]
end of trampoline     FF 25 00 00 00 00 <target+stolen>     jmp qword ptr [rip+0]
```

`kInlineHookPatchBytes = 14`. A relative call inside the displaced block becomes:

```
FF 15 02 00 00 00    call [rip+2]
EB 08                jmp +8
<destination 8 bytes>                      (16 bytes total)
```

Conditional branches become an inverted short `Jcc` over an absolute jump
(`0x70 | (cond ^ 1)`, length `kJumpBytes`, then the absolute jump).

## 2.2 Constants

| Constant | Value | Meaning |
| --- | --- | --- |
| `kInlineHookPatchBytes` | `14` | patch length / saved bytes (`InlineHook.h:12`) |
| `kCallBytes` | `16` | absolute call stub (`InlineHook.cpp:32`) |
| `kReach` | `0x7FFFFFFF` | 32-bit displacement reach (`InlineHook.cpp:36`) |
| `kSearchRange` | `0x7FFF0000` | trampoline search radius (just under 2 GB) (`InlineHook.cpp:40`) |
| `kMaxProbes` | `0x20000` (131072) | max granularity steps in the search (`InlineHook.cpp:44`) |

## 2.3 Public API

```cpp
namespace Avalonium {
class InlineHook {
public:
    InlineHook();
    ~InlineHook();                       // calls Remove()
    InlineHook(const InlineHook&) = delete;

    bool Install(void* target, void* detour, void** originalOut = nullptr,
                 std::string* reason = nullptr);
    void Remove();

    bool Installed() const;
    void* Original() const;              // the trampoline
    void* Target() const;
    void* Detour() const;
    size_t StolenBytes() const;

    const std::string& Summary() const;          // one line describing the install
    const std::string& PlacementNotes() const;   // operands that could not be carried
};
}
```

| Method | Behavior |
| --- | --- |
| `Install` | Decodes the prologue, plans each instruction's travel, reserves the trampoline, writes the trampoline, freezes other threads, writes the patch. Returns `false` with `*reason` set on any refusal. Sets `*originalOut` **before** the patch goes live |
| `Remove` | Restores the saved 14 bytes (threads frozen), clears all fields. The trampoline is deliberately **not** freed, a thread that entered it may still be running there; one page until process exit is cheaper than racing |
| `Summary()` | `displaced N byte(s) in M instruction(s), K re-addressed, C carried on a register; trampoline <addr> (0x… from the target, reach beyond 2GB would corrupt: required, and satisfied / not required); threads moved: n` |
| `PlacementNotes()` | Per refused operand: `the operand at +0xNN (bytes) is carried as it is: <reason>; the trampoline has to stay within reach of it` |

## 2.4 Why not MinHook

MinHook patches with a 5-byte **relative** jump to a relay it allocates, so the relay must be
within ±2 GB of the target. It searches a window around the target and, when that search comes
up empty, refuses the hook entirely (`MH_ERROR_MEMORY_ALLOC`). On real titles this happens even
when the window is *not* full: MinHook's first-fit walk only examines the slot below each
allocation base, so it can step straight past free gaps. Both of Avalonium's Lua hook targets
failed this way (2788 committed regions walked, no free page reported) while plain
`VirtualAlloc` trampolines for the same functions landed inside that same window.

Absolute jumps remove the requirement entirely.

## 2.5 Instruction travel (`Carry`)

| Kind | When | Encoding |
| --- | --- | --- |
| `Copy` | plain instruction | bytes copied; a RIP-relative displacement is rewritten (and validated) |
| `CopyThroughRegister` | RIP-relative operand that can be carried | `push r; mov r, imm64; <instruction addressing through r>; pop r` |
| `JumpAbs` | `E9`/`EB` | absolute jump |
| `CallAbs` | `E8` | absolute call stub |
| `JumpCondAbs` | `70..7F`, `0F 80..8F` | inverted short `Jcc` over an absolute jump |

`LOOP`/`JECXZ` (`E0..E3`) keep their relative form (`KeepsRelative`); an install refuses when
such a branch leaves the displaced block, or when an instruction that grew (an absolute
rewrite) is crossed between the loop and its destination.

## 2.6 RIP-relative carry

A RIP-relative operand is addressed from the instruction's own end via a 32-bit displacement,
so a copy only names the same address while it stays within ±2 GB of it. The hook therefore
**carries** the operand on a spare register:

```
push r          (0x50 + r)
mov r, imm64    (48 B8+r <8-byte address>)
<the instruction, addressing through r>   -- only the ModR/M byte changes
pop r           (0x58 + r)
```

* Registers offered: **rbx, rsi, rdi**, addressable without a REX prefix and never implied as
  operands by the instructions that carry no addressing byte.
* `push`/`pop`/`mov` leave the flags alone and the pair is balanced, so registers, stack
  pointer and flags are exactly as before; only the position changed.
* The rewritten ModR/M keeps the addressing mode and the operation's register/group bits and
  replaces only the base (`RIP → r`, no REX.B reinterpretation) and then copies the immediate
  bytes before `pop r`.

### Refusals (`RipCarryRefusal`): the operand keeps its own bytes

| Value | Reason text |
| --- | --- |
| `StackOrControl` | `it moves the stack pointer itself (a member of the FF group, or a pop)` |
| `UndecodedEncoding` | `it is a VEX or EVEX encoding, whose operands this disassembler does not decode` |
| `AddressSizePrefix` | `an address-size prefix makes its operand name the 32-bit EIP` |
| `DisplacementShape` | `its addressing byte is not the shape the rewrite re-encodes (a SIB byte, no 32-bit displacement, or a REX.B that would name the register in place of RIP)` |
| `NoFreeRegister` | `every register that could hold the address is one the instruction names` |

A refused operand still works **if** the trampoline lands within reach of the address it names;
that is what the placement window below is for, and what `PlacementNotes()` reports.

## 2.7 Trampoline placement

* **No refused operand** (`needsReach == false`): plain `VirtualAlloc`, no search.
* **Refused operands present**: the reachable window is computed as the intersection over each
  such operand of `D - kSearchRange - end` .. `D + kSearchRange + 1 - end` (where `D` is the
  operand's destination and `end` the copied instruction's end offset in the trampoline). If the
  window is empty → `the re-addressed operands leave nowhere within reach for a trampoline`.
* `ReserveNear` walks outward from the target in **allocation-granularity steps** (both
  directions), `VirtualQuery`-checking each candidate for `MEM_FREE` with `RegionSize >= size`,
  then `VirtualAlloc(MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE)`. Bounded by `kMaxProbes`.
  Failure message: `no free page within reach of an operand the prologue carries as it is (N allocation granularity steps walked); installing would corrupt it`.

## 2.8 Thread safety during install/remove

`ThreadFreeze`:

1. Snapshot all threads in the process (`CreateToolhelp32Snapshot`), excluding the caller.
2. Reserve the handle vector **before** suspending anything (a suspended thread may hold the
   process heap; allocating while threads are stopped could deadlock).
3. `OpenThread(THREAD_SUSPEND_RESUME|GET_CONTEXT|SET_CONTEXT)` + `SuspendThread`.
4. For any thread whose `Rip` lies inside the bytes being patched, move it to the **copy of the
   instruction it was about to execute** (moved only when `Rip` is exactly at an instruction
   start; otherwise left alone rather than moved to a guess). This is why the instruction
   layout is passed in: an instruction carried on a copy of itself may be longer than the
   original, so byte offsets do not line up.
5. Destructor resumes and closes every handle.

The patch write itself is `VirtualProtect(PAGE_EXECUTE_READWRITE)` → `WriteJump` → restore →
`FlushInstructionCache`, all inside the freeze.

## 2.9 Failure modes

| Message | Meaning |
| --- | --- |
| `a patch is already installed` | `Install` called twice |
| `no target or detour` | null arguments |
| `the instruction at +0x… could not be decoded` | the length disassembler refused |
| `the function ends before the patch fits` | a `ret`/`jmp r/m` before 14 bytes |
| `the prologue did not reach an end` | > 64 instructions without a terminator |
| `a LOOP/JECXZ in the prologue branches out of the block` | relative branch leaves the block |
| `a LOOP/JECXZ in the prologue is crossed by an instruction that grew` | loop target reached by an `end`-offset |
| `the re-addressed operands leave nowhere within reach for a trampoline` | empty placement window |
| `no free page within reach of an operand the prologue carries as it is (N …)` | `ReserveNear` exhausted |
| `VirtualAlloc failed` / `VirtualProtect failed on the target` | OS refusal |
| `the trampoline is out of reach of an operand it has to re-address` | final validation of the written displacement |

## 2.10 Consumers

* [`LuaHookManager`](../../Runtime/lua/LuaHookManager.cpp): the `ProcessEvent` and
  `ProcessLocalScriptFunction` patches (see [LuaAPI/HOOKS.md](../LuaAPI/HOOKS.md)).
* Any runtime code that needs a hook MinHook cannot place.

---

## Cross-references

* [README.md](README.md): overview and cost model.
* [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md): discovery code built on these APIs.
* [CryptoAPI/REFERENCE.md](../CryptoAPI/REFERENCE.md): bulk scans through `SafeMem_Read`.
* [LuaAPI/HOOKS.md](../LuaAPI/HOOKS.md): the hook targets and dispatch semantics.
