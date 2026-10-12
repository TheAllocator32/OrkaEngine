$input v_texcoord0, v_offset0, v_offset1, v_offset2

#include "bgfx_shader.sh"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa_common.glsl"

SAMPLER2D(s_colorTex, 0);

void main() {
    float4 offset[3];
    offset[0] = v_offset0;
    offset[1] = v_offset1;
    offset[2] = v_offset2;

    float2 edges = SMAALumaEdgeDetectionPS(v_texcoord0, offset, s_colorTex);
    ORKA_SMAA_FRAGCOLOR = vec4(edges, 0.0, 0.0);
}
