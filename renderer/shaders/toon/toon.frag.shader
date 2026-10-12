$input v_texcoord0, v_worldPos, v_normal

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

#define ORKA_MAX_TOON_LIGHTS 8

uniform vec4 u_lightPosRange[ORKA_MAX_TOON_LIGHTS];
uniform vec4 u_lightColorIntensity[ORKA_MAX_TOON_LIGHTS];
uniform vec4 u_lightDirType[ORKA_MAX_TOON_LIGHTS];
uniform vec4 u_lightSpot[ORKA_MAX_TOON_LIGHTS];
uniform vec4 u_lightParams;

float toonBand(float value) {
    float stepped = clamp(value, 0.0, 1.0) * 5.0;
    float lowerBand = floor(stepped);
    float edgeWidth = max(fwidth(stepped), 0.002);
    float edgeBlend = smoothstep(
        1.0 - edgeWidth,
        1.0,
        fract(stepped)
    );

    return clamp(
        (lowerBand + edgeBlend) / 5.0,
        0.0,
        1.0
    );
}

#define APPLY_TOON_LIGHT(LIGHT_INDEX) \
    { \
        float enabled = step( \
            float(LIGHT_INDEX) + 0.5, \
            u_lightParams.x \
        ); \
        vec3 toLight = \
            u_lightPosRange[LIGHT_INDEX].xyz - v_worldPos; \
        float distanceToLight = length(toLight); \
        float lightRange = max( \
            u_lightPosRange[LIGHT_INDEX].w, \
            0.001 \
        ); \
        vec3 lightDirection = \
            toLight / max(distanceToLight, 0.001); \
        float attenuation = clamp( \
            1.0 - distanceToLight / lightRange, \
            0.0, \
            1.0 \
        ); \
        attenuation *= attenuation; \
        float diffuse = max( \
            dot(normal, lightDirection), \
            0.0 \
        ); \
        vec3 spotVector = \
            u_lightDirType[LIGHT_INDEX].xyz; \
        vec3 spotDirection = spotVector / max( \
            length(spotVector), \
            0.001 \
        ); \
        float theta = dot( \
            -lightDirection, \
            spotDirection \
        ); \
        float innerCone = u_lightSpot[LIGHT_INDEX].x; \
        float outerCone = u_lightSpot[LIGHT_INDEX].y; \
        float coneFactor = clamp( \
            (theta - outerCone) / max( \
                innerCone - outerCone, \
                0.001 \
            ), \
            0.0, \
            1.0 \
        ); \
        float spotFactor = mix( \
            1.0, \
            coneFactor, \
            step( \
                0.5, \
                u_lightDirType[LIGHT_INDEX].w \
            ) \
        ); \
        vec3 lightColor = \
            u_lightColorIntensity[LIGHT_INDEX].rgb; \
        float intensity = \
            u_lightColorIntensity[LIGHT_INDEX].w; \
        float band = \
            toonBand(diffuse) \
            * attenuation \
            * spotFactor \
            * enabled; \
        lit += \
            baseColor.rgb \
            * lightColor \
            * intensity \
            * band; \
    }

void main() {
    vec4 baseColor = texture2D(
        s_texColor,
        v_texcoord0
    );

    vec3 normal = normalize(v_normal);
    vec3 lit = baseColor.rgb * u_lightParams.y;

    APPLY_TOON_LIGHT(0)
    APPLY_TOON_LIGHT(1)
    APPLY_TOON_LIGHT(2)
    APPLY_TOON_LIGHT(3)
    APPLY_TOON_LIGHT(4)
    APPLY_TOON_LIGHT(5)
    APPLY_TOON_LIGHT(6)
    APPLY_TOON_LIGHT(7)

    gl_FragColor = vec4(lit, baseColor.a);
}