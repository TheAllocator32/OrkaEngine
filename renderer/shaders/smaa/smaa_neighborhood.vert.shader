$input a_position, a_texcoord0
$output v_texcoord0, v_offset

#include "bgfx_shader.sh"
#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#include "smaa_common.glsl"

void main() {
    float2 texcoord = ORKA_SMAA_TEXCOORD(a_texcoord0);
    SMAANeighborhoodBlendingVS(texcoord, v_offset);

    v_texcoord0 = texcoord;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
