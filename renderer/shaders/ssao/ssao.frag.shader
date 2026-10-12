$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_depthTex, 0);

uniform vec4 u_ssaoParams;
uniform vec4 u_ssaoQuality;
uniform vec4 u_ssaoOptions;
uniform mat4 u_ssaoInvProjection;

vec3 reconstructViewPosition(vec2 uv, float depth) {
    float ndcDepth = depth * u_ssaoQuality.z + u_ssaoQuality.w;
    vec4 clip = vec4(uv * 2.0 - 1.0, ndcDepth, 1.0);
    vec4 view = mul(u_ssaoInvProjection, clip);
    return view.xyz / max(abs(view.w), 0.00001);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 rotateDir(vec2 direction, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec2(direction.x * c - direction.y * s, direction.x * s + direction.y * c);
}

vec3 reconstructNormal(vec2 uv, vec2 texel, vec3 centerPosition) {
    float depthRight = texture2D(s_depthTex, uv + vec2(texel.x, 0.0)).x;
    float depthLeft = texture2D(s_depthTex, uv - vec2(texel.x, 0.0)).x;
    float depthUp = texture2D(s_depthTex, uv + vec2(0.0, texel.y)).x;
    float depthDown = texture2D(s_depthTex, uv - vec2(0.0, texel.y)).x;

    vec3 positionRight = reconstructViewPosition(uv + vec2(texel.x, 0.0), depthRight);
    vec3 positionLeft = reconstructViewPosition(uv - vec2(texel.x, 0.0), depthLeft);
    vec3 positionUp = reconstructViewPosition(uv + vec2(0.0, texel.y), depthUp);
    vec3 positionDown = reconstructViewPosition(uv - vec2(0.0, texel.y), depthDown);

    vec3 normal = normalize(cross(positionRight - positionLeft, positionUp - positionDown));
    vec3 viewDirection = normalize(-centerPosition);
    if (dot(normal, viewDirection) < 0.0) {
        normal = -normal;
    }
    return normal;
}

float sampleOcclusion(vec2 uv, vec3 centerPosition, vec3 normal, vec2 direction, float radiusUv, float radius) {
    vec2 sampleUv = uv + direction * radiusUv;
    if (sampleUv.x <= 0.0 || sampleUv.y <= 0.0 || sampleUv.x >= 1.0 || sampleUv.y >= 1.0) {
        return 0.0;
    }

    float sampleDepth = texture2D(s_depthTex, sampleUv).x;
    if (sampleDepth >= 0.9999) {
        return 0.0;
    }

    vec3 samplePosition = reconstructViewPosition(sampleUv, sampleDepth);
    vec3 sampleVector = samplePosition - centerPosition;
    float sampleDistance = length(sampleVector);
    if (sampleDistance <= 0.0001) {
        return 0.0;
    }

    float normalWeight = max(dot(normal, sampleVector / sampleDistance) - u_ssaoQuality.x, 0.0);
    float rangeWeight = 1.0 - smoothstep(0.0, radius, sampleDistance);
    return normalWeight * rangeWeight;
}

void main() {
    float centerDepth = texture2D(s_depthTex, v_texcoord0).x;
    if (centerDepth >= 0.9999) {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    vec2 texel = u_ssaoParams.xy;
    float radius = u_ssaoParams.z;
    float strength = u_ssaoParams.w;
    vec3 centerPosition = reconstructViewPosition(v_texcoord0, centerDepth);
    vec3 normal = reconstructNormal(v_texcoord0, texel, centerPosition);
    float randomAngle = hash12(v_texcoord0 * vec2(1920.0, 1080.0)) * 6.2831853;
    float sampleRadiusPixels = clamp((radius / max(abs(centerPosition.z), 0.25)) * 260.0, 5.0, u_ssaoQuality.y);
    float baseRadiusUv = max(texel.x, texel.y) * sampleRadiusPixels;

    float occlusion = 0.0;
    float sampleCount = 16.0;
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 1.000,  0.000), randomAngle), baseRadiusUv * 0.35, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.707,  0.707), randomAngle), baseRadiusUv * 0.45, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.000,  1.000), randomAngle), baseRadiusUv * 0.55, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.707,  0.707), randomAngle), baseRadiusUv * 0.65, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-1.000,  0.000), randomAngle), baseRadiusUv * 0.75, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.707, -0.707), randomAngle), baseRadiusUv * 0.85, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.000, -1.000), randomAngle), baseRadiusUv * 0.95, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.707, -0.707), randomAngle), baseRadiusUv * 1.05, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.923,  0.382), randomAngle), baseRadiusUv * 1.15, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.382,  0.923), randomAngle), baseRadiusUv * 1.25, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.382,  0.923), randomAngle), baseRadiusUv * 1.35, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.923,  0.382), randomAngle), baseRadiusUv * 1.45, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.923, -0.382), randomAngle), baseRadiusUv * 1.55, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.382, -0.923), randomAngle), baseRadiusUv * 1.65, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.382, -0.923), randomAngle), baseRadiusUv * 1.75, radius);
    occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.923, -0.382), randomAngle), baseRadiusUv * 1.85, radius);

    if (u_ssaoOptions.x > 16.5) {
        sampleCount = 32.0;
        float angle2 = randomAngle + 0.3926991;
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.981,  0.195), angle2), baseRadiusUv * 0.40, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.831,  0.556), angle2), baseRadiusUv * 0.50, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.556,  0.831), angle2), baseRadiusUv * 0.60, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.195,  0.981), angle2), baseRadiusUv * 0.70, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.195,  0.981), angle2), baseRadiusUv * 0.80, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.556,  0.831), angle2), baseRadiusUv * 0.90, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.831,  0.556), angle2), baseRadiusUv * 1.00, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.981,  0.195), angle2), baseRadiusUv * 1.10, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.981, -0.195), angle2), baseRadiusUv * 1.20, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.831, -0.556), angle2), baseRadiusUv * 1.30, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.556, -0.831), angle2), baseRadiusUv * 1.40, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2(-0.195, -0.981), angle2), baseRadiusUv * 1.50, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.195, -0.981), angle2), baseRadiusUv * 1.60, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.556, -0.831), angle2), baseRadiusUv * 1.70, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.831, -0.556), angle2), baseRadiusUv * 1.80, radius);
        occlusion += sampleOcclusion(v_texcoord0, centerPosition, normal, rotateDir(vec2( 0.981, -0.195), angle2), baseRadiusUv * 1.90, radius);
    }

    float ao = clamp(1.0 - (occlusion / sampleCount) * strength, 0.22, 1.0);
    gl_FragColor = vec4(ao, ao, ao, 1.0);
}
