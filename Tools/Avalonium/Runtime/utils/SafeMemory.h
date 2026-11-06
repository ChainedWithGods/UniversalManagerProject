#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int SafeMem_IsProbeActive(void);

int SafeMem_IsKernelProbeAvailable(void);

void SafeMem_SetFastMode(int fast);

void SafeMem_ClearCache(void);

void SafeMem_PrefetchRegion(const void* addr, size_t size);

int SafeMem_IsReadable(const void* addr, size_t size);

int SafeMem_IsMapped(const void* addr, size_t size);

int SafeMem_VerifyPointer(const void* addr, size_t size);

void* SafeMem_ReadPtr(const void* addr);

int SafeMem_Read(const void* addr, void* out, size_t size);

size_t SafeMem_ReadSpan(const void* addr, void* out, size_t size);

size_t SafeMem_ReadSpanExact(const void* addr, void* out, size_t size);

int SafeMem_Write(void* dst, const void* src, size_t size);

int SafeMem_Compare(const void* addr, const void* expected, size_t size);

uint8_t  SafeMem_ReadU8(const void* addr, int* ok);
uint16_t SafeMem_ReadU16(const void* addr, int* ok);
uint32_t SafeMem_ReadU32(const void* addr, int* ok);
int32_t  SafeMem_ReadI32(const void* addr, int* ok);
uint64_t SafeMem_ReadU64(const void* addr, int* ok);
int64_t  SafeMem_ReadI64(const void* addr, int* ok);
float    SafeMem_ReadFloat(const void* addr, int* ok);
double   SafeMem_ReadDouble(const void* addr, int* ok);

size_t SafeMem_ReadString(const void* addr, char* out, size_t maxLen);

size_t SafeMem_ReadStringFast(const void* addr, char* out, size_t maxLen);

typedef int (*SafeExecFunc)(void* context);
int SafeMem_Execute(SafeExecFunc func, void* context);

int SafeMem_CallKeyFunction(const void* fn_ptr, unsigned char* out_buf);

void* SafeMem_CallMallocVftable(const void* instance, size_t size,
                                uint32_t alignment, int32_t mallocSlotIndex);

#ifdef __cplusplus
}

inline bool SafeMem_IsCanonical(const void* addr) {
  const uintptr_t a = reinterpret_cast<uintptr_t>(addr);
  return (a >= 0x10000ULL && a <= 0x00007FFFFFFEFFFFULL);
}

inline bool IsReadable(const void *addr, size_t size = sizeof(void *)) {
  return SafeMem_IsReadable(addr, size) != 0;
}

inline bool IsMapped(const void *addr, size_t size = sizeof(void *)) {
  return SafeMem_IsMapped(addr, size) != 0;
}

struct FastMemScope {
  FastMemScope()  { SafeMem_SetFastMode(1); }
  ~FastMemScope() { SafeMem_SetFastMode(0); }
};

#endif
