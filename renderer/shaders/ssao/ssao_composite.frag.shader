$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_sceneTex, 0);
SAMPLER2D(s_aoTex, 1);

void main() {
    vec4 color = texture2D(s_sceneTex, v_texcoord0);
    float ao = texture2D(s_aoTex, v_texcoord0).x;
    gl_FragColor = vec4(color.rgb * ao, color.a);
}
