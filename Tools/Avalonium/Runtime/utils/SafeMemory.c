#include "SafeMemory.h"
#include <stdlib.h>
#include <string.h>

typedef LONG NTSTATUS;
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_ACCESS_VIOLATION
#define STATUS_ACCESS_VIOLATION ((NTSTATUS)0xC0000005L)
#endif
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef NTSTATUS(NTAPI *pfnNtReadVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToRead,
    PSIZE_T NumberOfBytesReaded
);

typedef NTSTATUS(NTAPI *pfnNtWriteVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToWrite,
    PSIZE_T NumberOfBytesWritten
);

typedef NTSTATUS(NTAPI *pfnNtQueryVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    int MemoryInformationClass,
    PVOID MemoryInformation,
    SIZE_T MemoryInformationLength,
    PSIZE_T ReturnLength
);

static pfnNtReadVirtualMemory  g_pfnNtReadVirtualMemory  = NULL;
static pfnNtWriteVirtualMemory g_pfnNtWriteVirtualMemory = NULL;
static pfnNtQueryVirtualMemory g_pfnNtQueryVirtualMemory = NULL;
static volatile LONG g_NtApisInitialized = 0;

static size_t SafeMem_MinSize(size_t a, size_t b) { return a < b ? a : b; }
static uintptr_t SafeMem_MinAddr(uintptr_t a, uintptr_t b) { return a < b ? a : b; }

static void SafeMem_InitNtApis(void) {
  if (InterlockedCompareExchange(&g_NtApisInitialized, 1, 0) == 0) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
      g_pfnNtReadVirtualMemory  = (pfnNtReadVirtualMemory)GetProcAddress(hNtdll, "NtReadVirtualMemory");
      g_pfnNtWriteVirtualMemory = (pfnNtWriteVirtualMemory)GetProcAddress(hNtdll, "NtWriteVirtualMemory");
      g_pfnNtQueryVirtualMemory = (pfnNtQueryVirtualMemory)GetProcAddress(hNtdll, "NtQueryVirtualMemory");
    }
  }
}

int SafeMem_IsKernelProbeAvailable(void) {
  SafeMem_InitNtApis();
  return (g_pfnNtReadVirtualMemory != NULL);
}

__declspec(thread) int g_SafeMemProbeActive = 0;
static __declspec(thread) int g_SafeMemFastModeDepth = 0;

int SafeMem_IsProbeActive(void) {
  return g_SafeMemProbeActive;
}

static inline int IsCanonicalUserPtr(uintptr_t a) {
  return (a >= 0x10000ULL && a <= 0x00007FFFFFFEFFFFULL);
}

#define SAFEMEM_L1_CACHE_SIZE 256
#define SAFEMEM_L1_CACHE_MASK (SAFEMEM_L1_CACHE_SIZE - 1)
#define SAFEMEM_PAGE_SHIFT 12
#define SAFEMEM_PAGE_SIZE 4096ULL
#define SAFEMEM_PAGE_MASK (~(SAFEMEM_PAGE_SIZE - 1))

typedef struct {
  uintptr_t PageBase;
  uint8_t   Status;
} SafeMemL1Entry;

static __declspec(thread) SafeMemL1Entry g_L1Cache[SAFEMEM_L1_CACHE_SIZE] = {0};

static inline int SafeMem_L1Lookup(uintptr_t pageBase, uint8_t *outStatus) {
  size_t slot = (pageBase >> SAFEMEM_PAGE_SHIFT) & SAFEMEM_L1_CACHE_MASK;
  if (g_L1Cache[slot].PageBase == pageBase) {
    *outStatus = g_L1Cache[slot].Status;
    return 1;
  }
  return 0;
}

static inline void SafeMem_L1Insert(uintptr_t pageBase, uint8_t status) {
  size_t slot = (pageBase >> SAFEMEM_PAGE_SHIFT) & SAFEMEM_L1_CACHE_MASK;
  g_L1Cache[slot].PageBase = pageBase;
  g_L1Cache[slot].Status = status;
}

void SafeMem_ClearCache(void) {
  memset(g_L1Cache, 0, sizeof(g_L1Cache));
}

typedef struct {
  uintptr_t Start;
  uintptr_t End;
} SafeMemMapRegion;

static SafeMemMapRegion *volatile g_RegionMap = NULL;
static size_t g_RegionMapCapacity = 0;
static volatile LONG g_RegionMapCount = 0;
static volatile LONG g_RegionMapBuiltTick = 0;
static SRWLOCK g_RegionMapLock = SRWLOCK_INIT;

#define SAFEMEM_REGION_MAP_MIN_REBUILD_MS 30000

static int SafeMem_RegionAt(uintptr_t addr, uintptr_t *outStart, uintptr_t *outEnd) {
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
    return 0;
  if (mbi.State != MEM_COMMIT)
    return 0;
  const DWORD prot = mbi.Protect & 0xFF;
  if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE || (mbi.Protect & PAGE_GUARD))
    return 0;
  *outStart = (uintptr_t)mbi.BaseAddress;
  *outEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
  return 1;
}

static void SafeMem_RebuildRegionMap(void) {
  AcquireSRWLockExclusive(&g_RegionMapLock);

  if (g_RegionMapCount > 0 &&
      (LONG)(GetTickCount64() - g_RegionMapBuiltTick) < SAFEMEM_REGION_MAP_MIN_REBUILD_MS) {
    ReleaseSRWLockExclusive(&g_RegionMapLock);
    return;
  }

  const uintptr_t limit = 0x00007FFFFFFEFFFFULL;
  MEMORY_BASIC_INFORMATION mbi;

  size_t capacity = g_RegionMapCapacity > 0 ? g_RegionMapCapacity : 2048;
  SafeMemMapRegion *target = (SafeMemMapRegion *)malloc(capacity * sizeof(SafeMemMapRegion));
  if (!target) {
    ReleaseSRWLockExclusive(&g_RegionMapLock);
    return;
  }

  size_t count = 0;
  uintptr_t addr = 0x10000ULL;
  while (addr < limit) {
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
      break;
    const uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (regionEnd <= addr)
      break;
    if (mbi.State == MEM_COMMIT) {
      const DWORD prot = mbi.Protect & 0xFF;
      if (prot != PAGE_NOACCESS && prot != PAGE_EXECUTE && !(mbi.Protect & PAGE_GUARD)) {
        if (count >= capacity) {
          capacity *= 2;
          SafeMemMapRegion *grown = (SafeMemMapRegion *)realloc(target, capacity * sizeof(SafeMemMapRegion));
          if (!grown) {
            free(target);
            ReleaseSRWLockExclusive(&g_RegionMapLock);
            return;
          }
          target = grown;
        }
        target[count].Start = (uintptr_t)mbi.BaseAddress;
        target[count].End = regionEnd;
        ++count;
      }
    }
    addr = regionEnd;
  }

  if (count == 0) {
    free(target);
    ReleaseSRWLockExclusive(&g_RegionMapLock);
    return;
  }

  g_RegionMap = target;
  g_RegionMapCapacity = capacity;
  g_RegionMapBuiltTick = (LONG)GetTickCount64();
  g_RegionMapCount = (LONG)count;
  ReleaseSRWLockExclusive(&g_RegionMapLock);
}

void SafeMem_SetFastMode(int fast) {
  if (fast) {
    if (++g_SafeMemFastModeDepth == 1) {
      if (g_RegionMapCount == 0) {
        SafeMem_RebuildRegionMap();
      }
    }
  } else if (g_SafeMemFastModeDepth > 0) {
    --g_SafeMemFastModeDepth;
  }
}

static int SafeMem_RegionMapCovers(uintptr_t start, uintptr_t end) {
  const SafeMemMapRegion *map = g_RegionMap;
  const int mapCount = g_RegionMapCount;
  if (!map || mapCount <= 0)
    return 0;

  int lo = 0;
  int hi = mapCount - 1;
  int first = -1;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    if (start < map[mid].Start) {
      hi = mid - 1;
    } else {
      first = mid;
      lo = mid + 1;
    }
  }
  if (first < 0)
    return 0;

  uintptr_t covered = start;
  for (int i = first; i < mapCount && covered <= end; ++i) {
    if (map[i].Start > covered)
      break;
    if (map[i].End > covered)
      covered = map[i].End;
  }
  return covered > end;
}

int SafeMem_IsMapped(const void *addr, size_t size) {
  if (!addr || size == 0)
    return 0;
  const uintptr_t start = (uintptr_t)addr;
  const uintptr_t end = start + size - 1;
  if (!IsCanonicalUserPtr(start) || !IsCanonicalUserPtr(end) || end < start)
    return 0;

  if (SafeMem_RegionMapCovers(start, end))
    return 1;

  uintptr_t regionStart = 0;
  uintptr_t regionEnd = 0;
  if (!SafeMem_RegionAt(start, &regionStart, &regionEnd) || end >= regionEnd)
    return 0;

  return 1;
}

int SafeMem_IsReadable(const void *addr, size_t size) {
  return SafeMem_IsMapped(addr, size);
}

int SafeMem_VerifyPointer(const void *addr, size_t size) {
  if (!addr || size == 0) return 0;
  const uintptr_t a = (uintptr_t)addr;
  if (!IsCanonicalUserPtr(a) || !IsCanonicalUserPtr(a + size - 1)) return 0;
  return SafeMem_IsMapped(addr, size);
}

void SafeMem_PrefetchRegion(const void *addr, size_t size) {
  if (!addr || size == 0) return;
  const uintptr_t start = (uintptr_t)addr;
  const uintptr_t end = start + size;
  if (!IsCanonicalUserPtr(start) || !IsCanonicalUserPtr(end - 1)) return;

  uintptr_t p = start & SAFEMEM_PAGE_MASK;
  while (p < end) {
    SafeMem_L1Insert(p, 1);
    p += SAFEMEM_PAGE_SIZE;
  }
}

static int SafeMem_NtReadRaw(const void *addr, void *out, size_t size) {
  SafeMem_InitNtApis();
  if (g_pfnNtReadVirtualMemory) {
    SIZE_T bytesRead = 0;
    NTSTATUS status = g_pfnNtReadVirtualMemory(
        (HANDLE)(LONG_PTR)-1, (PVOID)addr, out, size, &bytesRead);
    return NT_SUCCESS(status) && (bytesRead == size);
  }

  SIZE_T bytesRead = 0;
  return ReadProcessMemory(GetCurrentProcess(), addr, out, size, &bytesRead) && (bytesRead == size);
}

void *SafeMem_ReadPtr(const void *addr) {
  const uintptr_t a = (uintptr_t)addr;
  if (!IsCanonicalUserPtr(a))
    return NULL;

  const uintptr_t pageBase = a & SAFEMEM_PAGE_MASK;

  uint8_t status = 0;
  if (SafeMem_L1Lookup(pageBase, &status)) {
    if (status & 1) {
      g_SafeMemProbeActive = 1;
      __try {
        void *res = *(void *const *)addr;
        g_SafeMemProbeActive = 0;
        return res;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_SafeMemProbeActive = 0;
        SafeMem_L1Insert(pageBase, 0);
        return NULL;
      }
    }
    return NULL;
  }

  if (SafeMem_IsMapped((const void *)pageBase, sizeof(void *))) {
    void *kernelRes = NULL;
    if (SafeMem_NtReadRaw(addr, &kernelRes, sizeof(kernelRes))) {
      SafeMem_L1Insert(pageBase, 1);
      return kernelRes;
    }
    SafeMem_L1Insert(pageBase, 0);
    return NULL;
  }

  void *kernelRes = NULL;
  if (SafeMem_NtReadRaw(addr, &kernelRes, sizeof(kernelRes))) {
    SafeMem_L1Insert(pageBase, 1);
    return kernelRes;
  }

  SafeMem_L1Insert(pageBase, 0);
  return NULL;
}

size_t SafeMem_ReadSpan(const void *addr, void *out, size_t size) {
  if (!addr || !out || size == 0)
    return 0;
  const uintptr_t start = (uintptr_t)addr;
  const uintptr_t end = start + size - 1;
  if (!IsCanonicalUserPtr(start) || !IsCanonicalUserPtr(end) || end < start)
    return 0;

  if (SafeMem_RegionMapCovers(start, end)) {
    g_SafeMemProbeActive = 1;
    __try {
      memcpy(out, addr, size);
      g_SafeMemProbeActive = 0;
      return size;
    } __except (EXCEPTION_EXECUTE_HANDLER) {

      g_SafeMemProbeActive = 0;
    }
  }

  size_t done = 0;
  while (done < size) {
    const uintptr_t cur = start + done;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)cur, &mbi, sizeof(mbi)) == 0)
      break;
    const uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (regionEnd <= cur)
      break;
    const DWORD prot = mbi.Protect & 0xFF;
    const int readable = (mbi.State == MEM_COMMIT) && prot != PAGE_NOACCESS &&
                         prot != PAGE_EXECUTE && !(mbi.Protect & PAGE_GUARD);
    if (!readable)
      break;
    const size_t runLen = (size_t)(SafeMem_MinAddr(regionEnd, end + 1) - cur);
    if (runLen == 0)
      break;
    g_SafeMemProbeActive = 1;
    __try {
      memcpy((uint8_t *)out + done, (const uint8_t *)cur, runLen);
      g_SafeMemProbeActive = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      g_SafeMemProbeActive = 0;
      break;
    }
    done += runLen;
  }
  if (done == size)
    return done;

  if (done == 0) {
    if (SafeMem_NtReadRaw(addr, out, size))
      return size;
    return 0;
  }
  return done;
}

size_t SafeMem_ReadSpanExact(const void *addr, void *out, size_t size) {
  const size_t got = SafeMem_ReadSpan(addr, out, size);
  return got == size ? size : 0;
}

int SafeMem_Read(const void *addr, void *out, size_t size) {
  if (!addr || !out || size == 0)
    return 0;
  const uintptr_t a = (uintptr_t)addr;
  const uintptr_t end = a + size - 1;
  if (!IsCanonicalUserPtr(a) || !IsCanonicalUserPtr(end) || end < a)
    return 0;

  const uintptr_t pageStart = a & SAFEMEM_PAGE_MASK;
  const uintptr_t pageEnd = end & SAFEMEM_PAGE_MASK;

  if (pageStart == pageEnd) {
    uint8_t status = 0;
    if (SafeMem_L1Lookup(pageStart, &status) && (status & 1)) {
      g_SafeMemProbeActive = 1;
      __try {
        memcpy(out, addr, size);
        g_SafeMemProbeActive = 0;
        return 1;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_SafeMemProbeActive = 0;
        SafeMem_L1Insert(pageStart, 0);
        return 0;
      }
    }

    if (SafeMem_IsMapped((const void *)pageStart, sizeof(void *))) {

      if (SafeMem_NtReadRaw(addr, out, size)) {
        SafeMem_L1Insert(pageStart, 1);
        return 1;
      }
      SafeMem_L1Insert(pageStart, 0);
      return 0;
    }

    if (SafeMem_NtReadRaw(addr, out, size)) {
      SafeMem_L1Insert(pageStart, 1);
      return 1;
    }

    SafeMem_L1Insert(pageStart, 0);
    return 0;
  }

  if (SafeMem_RegionMapCovers(a, end)) {
    if (SafeMem_NtReadRaw(addr, out, size))
      return 1;
    return 0;
  }

  uintptr_t p = pageStart;
  int allCached = 1;
  while (p <= pageEnd) {
    uint8_t st = 0;
    if (!SafeMem_L1Lookup(p, &st) || !(st & 1)) {
      allCached = 0;
      break;
    }
    p += SAFEMEM_PAGE_SIZE;
  }

  if (allCached) {
    g_SafeMemProbeActive = 1;
    __try {
      memcpy(out, addr, size);
      g_SafeMemProbeActive = 0;
      return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      g_SafeMemProbeActive = 0;
      return 0;
    }
  }

  if (SafeMem_NtReadRaw(addr, out, size)) {
    for (p = pageStart; p <= pageEnd; p += SAFEMEM_PAGE_SIZE) {
      SafeMem_L1Insert(p, 1);
    }
    return 1;
  }

  return 0;
}

uint8_t SafeMem_ReadU8(const void *addr, int *ok) {
  uint8_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

uint16_t SafeMem_ReadU16(const void *addr, int *ok) {
  uint16_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

uint32_t SafeMem_ReadU32(const void *addr, int *ok) {
  uint32_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

int32_t SafeMem_ReadI32(const void *addr, int *ok) {
  int32_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

uint64_t SafeMem_ReadU64(const void *addr, int *ok) {
  uint64_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

int64_t SafeMem_ReadI64(const void *addr, int *ok) {
  int64_t val = 0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

float SafeMem_ReadFloat(const void *addr, int *ok) {
  float val = 0.0f;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

double SafeMem_ReadDouble(const void *addr, int *ok) {
  double val = 0.0;
  int success = SafeMem_Read(addr, &val, sizeof(val));
  if (ok) *ok = success;
  return val;
}

size_t SafeMem_ReadString(const void *addr, char *out, size_t maxLen) {
  if (!addr || !out || maxLen == 0)
    return 0;
  out[0] = '\0';
  size_t count = 0;
  const char *p = (const char *)addr;
  while (count + 1 < maxLen) {
    char ch = 0;
    if (!SafeMem_Read(p + count, &ch, 1))
      break;
    out[count++] = ch;
    if (ch == '\0')
      return count - 1;
  }
  out[count] = '\0';
  return count;
}

size_t SafeMem_ReadStringFast(const void *addr, char *out, size_t maxLen) {
  if (!addr || !out || maxLen == 0)
    return 0;
  out[0] = '\0';
  if (maxLen == 1)
    return 0;

  char span[512];
  size_t count = 0;
  while (count + 1 < maxLen) {
    const size_t want = SafeMem_MinSize(sizeof(span), maxLen - 1 - count);
    const size_t got = SafeMem_ReadSpan((const uint8_t *)addr + count, span, want);
    if (got == 0)
      break;

    const char *nul = (const char *)memchr(span, '\0', got);
    const size_t take = nul ? (size_t)(nul - span) : got;
    memcpy(out + count, span, take);
    count += take;
    if (nul)
      break;
    if (got < want)
      break;
  }
  out[count] = '\0';
  return count;
}

int SafeMem_Write(void *dst, const void *src, size_t size) {
  if (!dst || !src || size == 0)
    return 0;
  const uintptr_t a = (uintptr_t)dst;
  const uintptr_t end = a + size - 1;
  if (!IsCanonicalUserPtr(a) || !IsCanonicalUserPtr(end) || end < a)
    return 0;

  g_SafeMemProbeActive = 1;
  __try {
    memcpy(dst, src, size);
    g_SafeMemProbeActive = 0;
    return 1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {

    DWORD oldProt = 0;
    if (VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProt)) {
      __try {
        memcpy(dst, src, size);
        VirtualProtect(dst, size, oldProt, &oldProt);
        g_SafeMemProbeActive = 0;
        return 1;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualProtect(dst, size, oldProt, &oldProt);
        g_SafeMemProbeActive = 0;
        return 0;
      }
    }
    g_SafeMemProbeActive = 0;

    SafeMem_InitNtApis();
    if (g_pfnNtWriteVirtualMemory) {
      SIZE_T written = 0;
      NTSTATUS st = g_pfnNtWriteVirtualMemory((HANDLE)(LONG_PTR)-1, dst, (PVOID)src, size, &written);
      return NT_SUCCESS(st) && (written == size);
    }
    return 0;
  }
}

int SafeMem_Compare(const void *addr, const void *expected, size_t size) {
  if (!addr || !expected || size == 0)
    return -1;
  const uintptr_t a = (uintptr_t)addr;
  const uintptr_t end = a + size - 1;
  if (!IsCanonicalUserPtr(a) || !IsCanonicalUserPtr(end) || end < a)
    return -1;

  if (size <= 256) {
    uint8_t stackBuf[256];
    if (!SafeMem_Read(addr, stackBuf, size))
      return -1;
    return memcmp(stackBuf, expected, size);
  }

  g_SafeMemProbeActive = 1;
  __try {
    int res = memcmp(addr, expected, size);
    g_SafeMemProbeActive = 0;
    return res;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    g_SafeMemProbeActive = 0;
    return -1;
  }
}

int SafeMem_Execute(SafeExecFunc func, void *context) {
  if (!func)
    return 1;

  g_SafeMemProbeActive = 1;
  __try {
    int res = func(context);
    g_SafeMemProbeActive = 0;
    return res;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    g_SafeMemProbeActive = 0;
    return 1;
  }
}

void* SafeMem_CallMallocVftable(const void* instance, size_t size,
                                uint32_t alignment, int32_t mallocSlotIndex) {
  if (!instance || !IsCanonicalUserPtr((uintptr_t)instance) || size == 0 || mallocSlotIndex < 0)
    return NULL;

  void **vtable = (void **)SafeMem_ReadPtr(instance);
  if (!vtable)
    return NULL;

  void *mallocSlot = SafeMem_ReadPtr(&vtable[mallocSlotIndex]);
  if (!mallocSlot || !IsCanonicalUserPtr((uintptr_t)mallocSlot))
    return NULL;

  __try {
    typedef void* (__fastcall* MallocFn)(const void* self, size_t count, uint32_t alignment);
    MallocFn mallocFn = (MallocFn)mallocSlot;
    void* block = mallocFn(instance, size, alignment);
    if (!block || !IsCanonicalUserPtr((uintptr_t)block))
      return NULL;

    unsigned char pattern = 0xA5;
    unsigned char readBack = 0;
    if (!SafeMem_Write(block, &pattern, sizeof(pattern)))
      return NULL;
    if (!SafeMem_Read(block, &readBack, sizeof(readBack)) || readBack != pattern)
      return NULL;
    return block;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return NULL;
  }
}

int SafeMem_CallKeyFunction(const void* fn_ptr, unsigned char* out_buf) {
  if (!fn_ptr || !out_buf)
    return 0;
  const uintptr_t a = (uintptr_t)fn_ptr;
  if (!IsCanonicalUserPtr(a))
    return 0;

  __try {
    typedef void (*KeyFn)(unsigned char*);
    KeyFn fn = (KeyFn)fn_ptr;
    fn(out_buf);
    return 1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}
