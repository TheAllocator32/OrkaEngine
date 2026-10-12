$input v_texcoord0, v_offset

#include "bgfx_shader.sh"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa_common.glsl"

SAMPLER2D(s_colorTex, 0);
SAMPLER2D(s_blendTex, 1);

void main() {
    ORKA_SMAA_FRAGCOLOR = SMAANeighborhoodBlendingPS(v_texcoord0, v_offset, s_colorTex, s_blendTex);
}
