$input v_texcoord0, v_pixcoord, v_offset0, v_offset1, v_offset2

#include "bgfx_shader.sh"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa_common.glsl"

SAMPLER2D(s_edgesTex, 0);
SAMPLER2D(s_areaTex, 1);
SAMPLER2D(s_searchTex, 2);

void main() {
    float4 offset[3];
    offset[0] = v_offset0;
    offset[1] = v_offset1;
    offset[2] = v_offset2;

    ORKA_SMAA_FRAGCOLOR = SMAABlendingWeightCalculationPS(
        v_texcoord0,
        v_pixcoord,
        offset,
        s_edgesTex,
        s_areaTex,
        s_searchTex,
        vec4(0.0, 0.0, 0.0, 0.0));
}
