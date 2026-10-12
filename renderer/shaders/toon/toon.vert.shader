$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_worldPos, v_normal

#include "bgfx_shader.sh"

void main() {
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    v_worldPos = worldPos.xyz;
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_texcoord0 = a_texcoord0;
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
