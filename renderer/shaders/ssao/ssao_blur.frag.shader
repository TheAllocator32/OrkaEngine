$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_aoTex, 0);
SAMPLER2D(s_depthTex, 1);

uniform vec4 u_ssaoParams;
uniform vec4 u_ssaoQuality;

vec2 blurSample(vec2 baseUv, vec2 offset, float gaussianWeight, float centerDepth) {
    vec2 uv = baseUv + u_ssaoParams.xy * offset;
    float sampleAo = texture2D(s_aoTex, uv).x;
    float sampleDepth = texture2D(s_depthTex, uv).x;
    float depthWeight = exp(-abs(sampleDepth - centerDepth) * 150.0);
    float weight = gaussianWeight * depthWeight;
    return vec2(sampleAo * weight, weight);
}

void main() {
    float centerDepth = texture2D(s_depthTex, v_texcoord0).x;
    vec2 sum = vec2(0.0, 0.0);

    sum += blurSample(v_texcoord0, vec2( 0.0,  0.0), 0.220, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 1.0,  0.0), 0.150, centerDepth);
    sum += blurSample(v_texcoord0, vec2(-1.0,  0.0), 0.150, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 0.0,  1.0), 0.150, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 0.0, -1.0), 0.150, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 1.0,  1.0), 0.090, centerDepth);
    sum += blurSample(v_texcoord0, vec2(-1.0,  1.0), 0.090, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 1.0, -1.0), 0.090, centerDepth);
    sum += blurSample(v_texcoord0, vec2(-1.0, -1.0), 0.090, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 2.0,  0.0), 0.050, centerDepth);
    sum += blurSample(v_texcoord0, vec2(-2.0,  0.0), 0.050, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 0.0,  2.0), 0.050, centerDepth);
    sum += blurSample(v_texcoord0, vec2( 0.0, -2.0), 0.050, centerDepth);

    float ao = sum.x / max(sum.y, 0.0001);
    gl_FragColor = vec4(ao, ao, ao, 1.0);
}
