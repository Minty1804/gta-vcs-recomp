// See gs_shaders.h. The headers come from src/render/CMakeLists.txt: DXBC from fxc (shaders/dxbc), SPIR-V from
// tools/hlsl_spirv.py (shaders/spirv), MSL text from tools/hlsl_msl.py (shaders/msl).
#include "render/gs_shaders.h"

#if defined(VCS_HAVE_D3D11)
using BYTE = unsigned char; // fxc's /Fh headers declare `const BYTE name[]`
#include "dxbc/gs_draw_vs.h"
#include "dxbc/gs_world_vs.h"
#include "dxbc/gs_lit_vs.h"
#include "dxbc/gs_draw_ps0.h"
#include "dxbc/gs_draw_ps0_dual.h"
#include "dxbc/gs_draw_ps1.h"
#include "dxbc/gs_draw_ps1_dual.h"
#include "dxbc/gs_draw_ps2.h"
#include "dxbc/gs_draw_ps2_dual.h"
#include "dxbc/gs_draw_ps3.h"
#include "dxbc/gs_draw_ps3_dual.h"
#include "dxbc/gs_draw_ps4.h"
#include "dxbc/gs_draw_ps4_dual.h"
#include "dxbc/gs_draw_ps5.h"
#include "dxbc/gs_draw_ps5_dual.h"
#include "dxbc/gs_draw_ps6.h"
#include "dxbc/gs_draw_ps6_dual.h"
#include "dxbc/gs_quad_vs.h"
#include "dxbc/gs_blit_ps.h"
#include "dxbc/gs_fill_color_ps.h"
#include "dxbc/gs_fill_depth_ps.h"
#include "dxbc/present_vs.h"
#include "dxbc/present_ps.h"
#include "dxbc/ui_vs.h"
#include "dxbc/ui_ps.h"
#include "dxbc/gs_draw_ps0_noz.h"
#include "dxbc/gs_draw_ps0_dual_noz.h"
#include "dxbc/gs_draw_ps1_noz.h"
#include "dxbc/gs_draw_ps1_dual_noz.h"
#include "dxbc/gs_draw_ps2_noz.h"
#include "dxbc/gs_draw_ps2_dual_noz.h"
#include "dxbc/gs_draw_ps3_noz.h"
#include "dxbc/gs_draw_ps3_dual_noz.h"
#include "dxbc/gs_draw_ps4_noz.h"
#include "dxbc/gs_draw_ps4_dual_noz.h"
#include "dxbc/gs_draw_ps5_noz.h"
#include "dxbc/gs_draw_ps5_dual_noz.h"
#include "dxbc/gs_draw_ps6_noz.h"
#include "dxbc/gs_draw_ps6_dual_noz.h"
#include "dxbc/gs_draw_ps0_nat.h"
#include "dxbc/gs_draw_ps0_dual_nat.h"
#include "dxbc/gs_draw_ps1_nat.h"
#include "dxbc/gs_draw_ps1_dual_nat.h"
#include "dxbc/gs_draw_ps2_nat.h"
#include "dxbc/gs_draw_ps2_dual_nat.h"
#include "dxbc/gs_draw_ps3_nat.h"
#include "dxbc/gs_draw_ps3_dual_nat.h"
#include "dxbc/gs_draw_ps4_nat.h"
#include "dxbc/gs_draw_ps4_dual_nat.h"
#include "dxbc/gs_draw_ps5_nat.h"
#include "dxbc/gs_draw_ps5_dual_nat.h"
#include "dxbc/gs_draw_ps6_nat.h"
#include "dxbc/gs_draw_ps6_dual_nat.h"
#endif
#if defined(VCS_HAVE_VULKAN)
#include "spirv/gs_draw_vs.h"
#include "spirv/gs_world_vs.h"
#include "spirv/gs_lit_vs.h"
#include "spirv/gs_draw_ps0.h"
#include "spirv/gs_draw_ps0_dual.h"
#include "spirv/gs_draw_ps1.h"
#include "spirv/gs_draw_ps1_dual.h"
#include "spirv/gs_draw_ps2.h"
#include "spirv/gs_draw_ps2_dual.h"
#include "spirv/gs_draw_ps3.h"
#include "spirv/gs_draw_ps3_dual.h"
#include "spirv/gs_draw_ps4.h"
#include "spirv/gs_draw_ps4_dual.h"
#include "spirv/gs_draw_ps5.h"
#include "spirv/gs_draw_ps5_dual.h"
#include "spirv/gs_draw_ps6.h"
#include "spirv/gs_draw_ps6_dual.h"
#include "spirv/gs_quad_vs.h"
#include "spirv/gs_blit_ps.h"
#include "spirv/gs_fill_color_ps.h"
#include "spirv/gs_fill_depth_ps.h"
#include "spirv/present_vs.h"
#include "spirv/present_ps.h"
#include "spirv/ui_vs.h"
#include "spirv/ui_ps.h"
#include "spirv/gs_draw_ps0_noz.h"
#include "spirv/gs_draw_ps0_dual_noz.h"
#include "spirv/gs_draw_ps1_noz.h"
#include "spirv/gs_draw_ps1_dual_noz.h"
#include "spirv/gs_draw_ps2_noz.h"
#include "spirv/gs_draw_ps2_dual_noz.h"
#include "spirv/gs_draw_ps3_noz.h"
#include "spirv/gs_draw_ps3_dual_noz.h"
#include "spirv/gs_draw_ps4_noz.h"
#include "spirv/gs_draw_ps4_dual_noz.h"
#include "spirv/gs_draw_ps5_noz.h"
#include "spirv/gs_draw_ps5_dual_noz.h"
#include "spirv/gs_draw_ps6_noz.h"
#include "spirv/gs_draw_ps6_dual_noz.h"
#include "spirv/gs_draw_ps0_nat.h"
#include "spirv/gs_draw_ps0_dual_nat.h"
#include "spirv/gs_draw_ps1_nat.h"
#include "spirv/gs_draw_ps1_dual_nat.h"
#include "spirv/gs_draw_ps2_nat.h"
#include "spirv/gs_draw_ps2_dual_nat.h"
#include "spirv/gs_draw_ps3_nat.h"
#include "spirv/gs_draw_ps3_dual_nat.h"
#include "spirv/gs_draw_ps4_nat.h"
#include "spirv/gs_draw_ps4_dual_nat.h"
#include "spirv/gs_draw_ps5_nat.h"
#include "spirv/gs_draw_ps5_dual_nat.h"
#include "spirv/gs_draw_ps6_nat.h"
#include "spirv/gs_draw_ps6_dual_nat.h"
#endif
#if defined(VCS_HAVE_METAL)
#include "msl/gs_draw_vs.h"
#include "msl/gs_world_vs.h"
#include "msl/gs_lit_vs.h"
#include "msl/gs_draw_ps0.h"
#include "msl/gs_draw_ps0_dual.h"
#include "msl/gs_draw_ps1.h"
#include "msl/gs_draw_ps1_dual.h"
#include "msl/gs_draw_ps2.h"
#include "msl/gs_draw_ps2_dual.h"
#include "msl/gs_draw_ps3.h"
#include "msl/gs_draw_ps3_dual.h"
#include "msl/gs_draw_ps4.h"
#include "msl/gs_draw_ps4_dual.h"
#include "msl/gs_draw_ps5.h"
#include "msl/gs_draw_ps5_dual.h"
#include "msl/gs_draw_ps6.h"
#include "msl/gs_draw_ps6_dual.h"
#include "msl/gs_quad_vs.h"
#include "msl/gs_blit_ps.h"
#include "msl/gs_fill_color_ps.h"
#include "msl/gs_fill_depth_ps.h"
#include "msl/present_vs.h"
#include "msl/present_ps.h"
#include "msl/ui_vs.h"
#include "msl/ui_ps.h"
#include "msl/gs_draw_ps0_noz.h"
#include "msl/gs_draw_ps0_dual_noz.h"
#include "msl/gs_draw_ps1_noz.h"
#include "msl/gs_draw_ps1_dual_noz.h"
#include "msl/gs_draw_ps2_noz.h"
#include "msl/gs_draw_ps2_dual_noz.h"
#include "msl/gs_draw_ps3_noz.h"
#include "msl/gs_draw_ps3_dual_noz.h"
#include "msl/gs_draw_ps4_noz.h"
#include "msl/gs_draw_ps4_dual_noz.h"
#include "msl/gs_draw_ps5_noz.h"
#include "msl/gs_draw_ps5_dual_noz.h"
#include "msl/gs_draw_ps6_noz.h"
#include "msl/gs_draw_ps6_dual_noz.h"
#include "msl/gs_draw_ps0_nat.h"
#include "msl/gs_draw_ps0_dual_nat.h"
#include "msl/gs_draw_ps1_nat.h"
#include "msl/gs_draw_ps1_dual_nat.h"
#include "msl/gs_draw_ps2_nat.h"
#include "msl/gs_draw_ps2_dual_nat.h"
#include "msl/gs_draw_ps3_nat.h"
#include "msl/gs_draw_ps3_dual_nat.h"
#include "msl/gs_draw_ps4_nat.h"
#include "msl/gs_draw_ps4_dual_nat.h"
#include "msl/gs_draw_ps5_nat.h"
#include "msl/gs_draw_ps5_dual_nat.h"
#include "msl/gs_draw_ps6_nat.h"
#include "msl/gs_draw_ps6_dual_nat.h"
#endif

namespace render
{
    const GsShaders &gsShaders(rhi::Backend backend)
    {
#if defined(VCS_HAVE_D3D11)
        static const GsShaders dxbc{
            {kGsDrawVS, sizeof(kGsDrawVS)},
            {kGsWorldVS, sizeof(kGsWorldVS)},
            {kGsLitVS, sizeof(kGsLitVS)},
            {
                {{kGsDrawPS0, sizeof(kGsDrawPS0)}, {kGsDrawPS0Dual, sizeof(kGsDrawPS0Dual)}},
                {{kGsDrawPS1, sizeof(kGsDrawPS1)}, {kGsDrawPS1Dual, sizeof(kGsDrawPS1Dual)}},
                {{kGsDrawPS2, sizeof(kGsDrawPS2)}, {kGsDrawPS2Dual, sizeof(kGsDrawPS2Dual)}},
                {{kGsDrawPS3, sizeof(kGsDrawPS3)}, {kGsDrawPS3Dual, sizeof(kGsDrawPS3Dual)}},
                {{kGsDrawPS4, sizeof(kGsDrawPS4)}, {kGsDrawPS4Dual, sizeof(kGsDrawPS4Dual)}},
                {{kGsDrawPS5, sizeof(kGsDrawPS5)}, {kGsDrawPS5Dual, sizeof(kGsDrawPS5Dual)}},
                {{kGsDrawPS6, sizeof(kGsDrawPS6)}, {kGsDrawPS6Dual, sizeof(kGsDrawPS6Dual)}},
            },
            {kGsQuadVS, sizeof(kGsQuadVS)},
            {kGsBlitPS, sizeof(kGsBlitPS)},
            {kGsFillColorPS, sizeof(kGsFillColorPS)},
            {kGsFillDepthPS, sizeof(kGsFillDepthPS)},
            {kPresentVS, sizeof(kPresentVS)},
            {kPresentPS, sizeof(kPresentPS)},
            {kUiVS, sizeof(kUiVS)},
            {kUiPS, sizeof(kUiPS)},
            {
                {{kGsDrawPS0NoZ, sizeof(kGsDrawPS0NoZ)}, {kGsDrawPS0DualNoZ, sizeof(kGsDrawPS0DualNoZ)}},
                {{kGsDrawPS1NoZ, sizeof(kGsDrawPS1NoZ)}, {kGsDrawPS1DualNoZ, sizeof(kGsDrawPS1DualNoZ)}},
                {{kGsDrawPS2NoZ, sizeof(kGsDrawPS2NoZ)}, {kGsDrawPS2DualNoZ, sizeof(kGsDrawPS2DualNoZ)}},
                {{kGsDrawPS3NoZ, sizeof(kGsDrawPS3NoZ)}, {kGsDrawPS3DualNoZ, sizeof(kGsDrawPS3DualNoZ)}},
                {{kGsDrawPS4NoZ, sizeof(kGsDrawPS4NoZ)}, {kGsDrawPS4DualNoZ, sizeof(kGsDrawPS4DualNoZ)}},
                {{kGsDrawPS5NoZ, sizeof(kGsDrawPS5NoZ)}, {kGsDrawPS5DualNoZ, sizeof(kGsDrawPS5DualNoZ)}},
                {{kGsDrawPS6NoZ, sizeof(kGsDrawPS6NoZ)}, {kGsDrawPS6DualNoZ, sizeof(kGsDrawPS6DualNoZ)}},
            },
            {
                {{kGsDrawPS0Nat, sizeof(kGsDrawPS0Nat)}, {kGsDrawPS0DualNat, sizeof(kGsDrawPS0DualNat)}},
                {{kGsDrawPS1Nat, sizeof(kGsDrawPS1Nat)}, {kGsDrawPS1DualNat, sizeof(kGsDrawPS1DualNat)}},
                {{kGsDrawPS2Nat, sizeof(kGsDrawPS2Nat)}, {kGsDrawPS2DualNat, sizeof(kGsDrawPS2DualNat)}},
                {{kGsDrawPS3Nat, sizeof(kGsDrawPS3Nat)}, {kGsDrawPS3DualNat, sizeof(kGsDrawPS3DualNat)}},
                {{kGsDrawPS4Nat, sizeof(kGsDrawPS4Nat)}, {kGsDrawPS4DualNat, sizeof(kGsDrawPS4DualNat)}},
                {{kGsDrawPS5Nat, sizeof(kGsDrawPS5Nat)}, {kGsDrawPS5DualNat, sizeof(kGsDrawPS5DualNat)}},
                {{kGsDrawPS6Nat, sizeof(kGsDrawPS6Nat)}, {kGsDrawPS6DualNat, sizeof(kGsDrawPS6DualNat)}},
            },
        };
        if (backend == rhi::Backend::D3D11 || backend == rhi::Backend::D3D12) // (DXBC runs on D3D12 as it is)
            return dxbc;
#endif
#if defined(VCS_HAVE_VULKAN)
        static const GsShaders spirv{
            {kGsDrawVS_spv, sizeof(kGsDrawVS_spv)},
            {kGsWorldVS_spv, sizeof(kGsWorldVS_spv)},
            {kGsLitVS_spv, sizeof(kGsLitVS_spv)},
            {
                {{kGsDrawPS0_spv, sizeof(kGsDrawPS0_spv)}, {kGsDrawPS0Dual_spv, sizeof(kGsDrawPS0Dual_spv)}},
                {{kGsDrawPS1_spv, sizeof(kGsDrawPS1_spv)}, {kGsDrawPS1Dual_spv, sizeof(kGsDrawPS1Dual_spv)}},
                {{kGsDrawPS2_spv, sizeof(kGsDrawPS2_spv)}, {kGsDrawPS2Dual_spv, sizeof(kGsDrawPS2Dual_spv)}},
                {{kGsDrawPS3_spv, sizeof(kGsDrawPS3_spv)}, {kGsDrawPS3Dual_spv, sizeof(kGsDrawPS3Dual_spv)}},
                {{kGsDrawPS4_spv, sizeof(kGsDrawPS4_spv)}, {kGsDrawPS4Dual_spv, sizeof(kGsDrawPS4Dual_spv)}},
                {{kGsDrawPS5_spv, sizeof(kGsDrawPS5_spv)}, {kGsDrawPS5Dual_spv, sizeof(kGsDrawPS5Dual_spv)}},
                {{kGsDrawPS6_spv, sizeof(kGsDrawPS6_spv)}, {kGsDrawPS6Dual_spv, sizeof(kGsDrawPS6Dual_spv)}},
            },
            {kGsQuadVS_spv, sizeof(kGsQuadVS_spv)},
            {kGsBlitPS_spv, sizeof(kGsBlitPS_spv)},
            {kGsFillColorPS_spv, sizeof(kGsFillColorPS_spv)},
            {kGsFillDepthPS_spv, sizeof(kGsFillDepthPS_spv)},
            {kPresentVS_spv, sizeof(kPresentVS_spv)},
            {kPresentPS_spv, sizeof(kPresentPS_spv)},
            {kUiVS_spv, sizeof(kUiVS_spv)},
            {kUiPS_spv, sizeof(kUiPS_spv)},
            {
                {{kGsDrawPS0NoZ_spv, sizeof(kGsDrawPS0NoZ_spv)}, {kGsDrawPS0DualNoZ_spv, sizeof(kGsDrawPS0DualNoZ_spv)}},
                {{kGsDrawPS1NoZ_spv, sizeof(kGsDrawPS1NoZ_spv)}, {kGsDrawPS1DualNoZ_spv, sizeof(kGsDrawPS1DualNoZ_spv)}},
                {{kGsDrawPS2NoZ_spv, sizeof(kGsDrawPS2NoZ_spv)}, {kGsDrawPS2DualNoZ_spv, sizeof(kGsDrawPS2DualNoZ_spv)}},
                {{kGsDrawPS3NoZ_spv, sizeof(kGsDrawPS3NoZ_spv)}, {kGsDrawPS3DualNoZ_spv, sizeof(kGsDrawPS3DualNoZ_spv)}},
                {{kGsDrawPS4NoZ_spv, sizeof(kGsDrawPS4NoZ_spv)}, {kGsDrawPS4DualNoZ_spv, sizeof(kGsDrawPS4DualNoZ_spv)}},
                {{kGsDrawPS5NoZ_spv, sizeof(kGsDrawPS5NoZ_spv)}, {kGsDrawPS5DualNoZ_spv, sizeof(kGsDrawPS5DualNoZ_spv)}},
                {{kGsDrawPS6NoZ_spv, sizeof(kGsDrawPS6NoZ_spv)}, {kGsDrawPS6DualNoZ_spv, sizeof(kGsDrawPS6DualNoZ_spv)}},
            },
            {
                {{kGsDrawPS0Nat_spv, sizeof(kGsDrawPS0Nat_spv)}, {kGsDrawPS0DualNat_spv, sizeof(kGsDrawPS0DualNat_spv)}},
                {{kGsDrawPS1Nat_spv, sizeof(kGsDrawPS1Nat_spv)}, {kGsDrawPS1DualNat_spv, sizeof(kGsDrawPS1DualNat_spv)}},
                {{kGsDrawPS2Nat_spv, sizeof(kGsDrawPS2Nat_spv)}, {kGsDrawPS2DualNat_spv, sizeof(kGsDrawPS2DualNat_spv)}},
                {{kGsDrawPS3Nat_spv, sizeof(kGsDrawPS3Nat_spv)}, {kGsDrawPS3DualNat_spv, sizeof(kGsDrawPS3DualNat_spv)}},
                {{kGsDrawPS4Nat_spv, sizeof(kGsDrawPS4Nat_spv)}, {kGsDrawPS4DualNat_spv, sizeof(kGsDrawPS4DualNat_spv)}},
                {{kGsDrawPS5Nat_spv, sizeof(kGsDrawPS5Nat_spv)}, {kGsDrawPS5DualNat_spv, sizeof(kGsDrawPS5DualNat_spv)}},
                {{kGsDrawPS6Nat_spv, sizeof(kGsDrawPS6Nat_spv)}, {kGsDrawPS6DualNat_spv, sizeof(kGsDrawPS6DualNat_spv)}},
            },
        };
        if (backend == rhi::Backend::Vulkan)
            return spirv;
#endif
#if defined(VCS_HAVE_METAL)
        static const GsShaders msl{
            {kGsDrawVS_msl, sizeof(kGsDrawVS_msl)},
            {kGsWorldVS_msl, sizeof(kGsWorldVS_msl)},
            {kGsLitVS_msl, sizeof(kGsLitVS_msl)},
            {
                {{kGsDrawPS0_msl, sizeof(kGsDrawPS0_msl)}, {kGsDrawPS0Dual_msl, sizeof(kGsDrawPS0Dual_msl)}},
                {{kGsDrawPS1_msl, sizeof(kGsDrawPS1_msl)}, {kGsDrawPS1Dual_msl, sizeof(kGsDrawPS1Dual_msl)}},
                {{kGsDrawPS2_msl, sizeof(kGsDrawPS2_msl)}, {kGsDrawPS2Dual_msl, sizeof(kGsDrawPS2Dual_msl)}},
                {{kGsDrawPS3_msl, sizeof(kGsDrawPS3_msl)}, {kGsDrawPS3Dual_msl, sizeof(kGsDrawPS3Dual_msl)}},
                {{kGsDrawPS4_msl, sizeof(kGsDrawPS4_msl)}, {kGsDrawPS4Dual_msl, sizeof(kGsDrawPS4Dual_msl)}},
                {{kGsDrawPS5_msl, sizeof(kGsDrawPS5_msl)}, {kGsDrawPS5Dual_msl, sizeof(kGsDrawPS5Dual_msl)}},
                {{kGsDrawPS6_msl, sizeof(kGsDrawPS6_msl)}, {kGsDrawPS6Dual_msl, sizeof(kGsDrawPS6Dual_msl)}},
            },
            {kGsQuadVS_msl, sizeof(kGsQuadVS_msl)},
            {kGsBlitPS_msl, sizeof(kGsBlitPS_msl)},
            {kGsFillColorPS_msl, sizeof(kGsFillColorPS_msl)},
            {kGsFillDepthPS_msl, sizeof(kGsFillDepthPS_msl)},
            {kPresentVS_msl, sizeof(kPresentVS_msl)},
            {kPresentPS_msl, sizeof(kPresentPS_msl)},
            {kUiVS_msl, sizeof(kUiVS_msl)},
            {kUiPS_msl, sizeof(kUiPS_msl)},
            {
                {{kGsDrawPS0NoZ_msl, sizeof(kGsDrawPS0NoZ_msl)}, {kGsDrawPS0DualNoZ_msl, sizeof(kGsDrawPS0DualNoZ_msl)}},
                {{kGsDrawPS1NoZ_msl, sizeof(kGsDrawPS1NoZ_msl)}, {kGsDrawPS1DualNoZ_msl, sizeof(kGsDrawPS1DualNoZ_msl)}},
                {{kGsDrawPS2NoZ_msl, sizeof(kGsDrawPS2NoZ_msl)}, {kGsDrawPS2DualNoZ_msl, sizeof(kGsDrawPS2DualNoZ_msl)}},
                {{kGsDrawPS3NoZ_msl, sizeof(kGsDrawPS3NoZ_msl)}, {kGsDrawPS3DualNoZ_msl, sizeof(kGsDrawPS3DualNoZ_msl)}},
                {{kGsDrawPS4NoZ_msl, sizeof(kGsDrawPS4NoZ_msl)}, {kGsDrawPS4DualNoZ_msl, sizeof(kGsDrawPS4DualNoZ_msl)}},
                {{kGsDrawPS5NoZ_msl, sizeof(kGsDrawPS5NoZ_msl)}, {kGsDrawPS5DualNoZ_msl, sizeof(kGsDrawPS5DualNoZ_msl)}},
                {{kGsDrawPS6NoZ_msl, sizeof(kGsDrawPS6NoZ_msl)}, {kGsDrawPS6DualNoZ_msl, sizeof(kGsDrawPS6DualNoZ_msl)}},
            },
            {
                {{kGsDrawPS0Nat_msl, sizeof(kGsDrawPS0Nat_msl)}, {kGsDrawPS0DualNat_msl, sizeof(kGsDrawPS0DualNat_msl)}},
                {{kGsDrawPS1Nat_msl, sizeof(kGsDrawPS1Nat_msl)}, {kGsDrawPS1DualNat_msl, sizeof(kGsDrawPS1DualNat_msl)}},
                {{kGsDrawPS2Nat_msl, sizeof(kGsDrawPS2Nat_msl)}, {kGsDrawPS2DualNat_msl, sizeof(kGsDrawPS2DualNat_msl)}},
                {{kGsDrawPS3Nat_msl, sizeof(kGsDrawPS3Nat_msl)}, {kGsDrawPS3DualNat_msl, sizeof(kGsDrawPS3DualNat_msl)}},
                {{kGsDrawPS4Nat_msl, sizeof(kGsDrawPS4Nat_msl)}, {kGsDrawPS4DualNat_msl, sizeof(kGsDrawPS4DualNat_msl)}},
                {{kGsDrawPS5Nat_msl, sizeof(kGsDrawPS5Nat_msl)}, {kGsDrawPS5DualNat_msl, sizeof(kGsDrawPS5DualNat_msl)}},
                {{kGsDrawPS6Nat_msl, sizeof(kGsDrawPS6Nat_msl)}, {kGsDrawPS6DualNat_msl, sizeof(kGsDrawPS6DualNat_msl)}},
            },
        };
        if (backend == rhi::Backend::Metal)
            return msl;
#endif
        (void)backend;
        static const GsShaders none{};
        return none;
    }
}
