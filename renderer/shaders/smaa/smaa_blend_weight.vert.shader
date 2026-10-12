$input a_position, a_texcoord0
$output v_texcoord0, v_pixcoord, v_offset0, v_offset1, v_offset2

#include "bgfx_shader.sh"
#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#include "smaa_common.glsl"

void main() {
    float2 texcoord = ORKA_SMAA_TEXCOORD(a_texcoord0);
    float4 offset[3];
    SMAABlendingWeightCalculationVS(texcoord, v_pixcoord, offset);

    v_texcoord0 = texcoord;
    v_offset0 = offset[0];
    v_offset1 = offset[1];
    v_offset2 = offset[2];

    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
