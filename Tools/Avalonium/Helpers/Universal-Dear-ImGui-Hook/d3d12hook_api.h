#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool D3D12Hook_TextureBridgeReady();

uint32_t D3D12Hook_TextureEpoch();

uint64_t D3D12Hook_CreateTextureView(void* resource, bool srgb);

void D3D12Hook_ReleaseTextureView(uint64_t view);

#ifdef __cplusplus
}
#endif
