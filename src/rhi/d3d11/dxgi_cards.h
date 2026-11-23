// [Minty1804 fork] The DXGI card list the D3D11 backend keeps (Graphics > Graphics card, GitHub issue 112), shared with
// the D3D12 backend so both APIs take the same card for the same rhi::Gpu id. Windows only; only the two DXGI backends
// include it.
#pragma once

#include <dxgi1_6.h>
#include <wrl/client.h>

namespace rhi
{
    // The card with this id, else the fastest (DXGI's high-performance order); null when DXGI lists none. Logs the
    // cards and the one taken (rhi_d3d11.cpp pickAdapter).
    Microsoft::WRL::ComPtr<IDXGIAdapter1> pickDxgiAdapter(int id);
}
