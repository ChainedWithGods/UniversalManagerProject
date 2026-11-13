#pragma once

#include <stdint.h>

/*
    Texture views for the DX12 overlay backend.

    The ImGui DX12 backend binds each draw's texture as a D3D12 GPU descriptor
    handle. Mod textures therefore need a descriptor in the overlay's
    shader-visible SRV heap, which this hook owns and binds every frame.
*/

#ifdef __cplusplus
extern "C" {
#endif

/* True when the DX12 overlay backend owns a device and its SRV heap. */
bool D3D12Hook_TextureBridgeReady();

/* Changes whenever the SRV heap is recreated; cached views do not survive it. */
uint32_t D3D12Hook_TextureEpoch();

/*
    Creates a shader resource view for a live ID3D12Resource in the overlay's
    heap and returns its GPU descriptor handle, or 0 when the backend is not
    active or the resource cannot be viewed.
*/
uint64_t D3D12Hook_CreateTextureView(void* resource);

#ifdef __cplusplus
}
#endif