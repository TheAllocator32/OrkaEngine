/*
 * File: Light.hpp
 *
 * Contains the sacred Light.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../core/Color.hpp"

enum LightType {
    LT_POINT,
    LT_SPOT
};

// This container class matches perfectly to .light.whaleml... So I was to lazy to write function docs
class Light {
public:
    Light() = default;
    Light(Color color, float intensity, LightType type, float range, float spotInnerConeAngle=0, float spotOuterConeAngle=0, bool enableShadows=true, int shadowResolution=1024) : m_color(color), m_intensity(intensity), m_type(type), m_range(range), m_spotInnerConeAngle(spotInnerConeAngle), m_spotOuterConeAngle(spotOuterConeAngle), m_enableShadows(enableShadows), m_shadowResolution(shadowResolution) {}
    Light(Color color, float intensity, LightType type, float range, bool enableShadows=true, int shadowResolution=1024) : Light(color, intensity, type, range, 0, 0, enableShadows, shadowResolution) {}

    void setLightType(LightType type) { m_type = type; }
    void setColor(Color color) { m_color = color; }
    void setIntensity(float intensity) { m_intensity = intensity; }
    void setRange(float range) { m_range = range; }
    void setSpotInnerConeAngle(float angle) { m_spotInnerConeAngle = angle; }
    void setSpotOuterConeAngle(float angle) { m_spotOuterConeAngle = angle; }
    void setEnableShadows(bool enable) { m_enableShadows = enable; }
    void setShadowResolution(int resolution) { m_shadowResolution = resolution; }

    inline LightType getType() const { return m_type; }
    inline float getRange() const { return m_range; }
    inline float getSpotInnerConeAngle() const { return m_spotInnerConeAngle; }
    inline float getSpotOuterConeAngle() const { return m_spotOuterConeAngle; }
    inline bool getEnableShadows() const { return m_enableShadows; }
    inline int getShadowResolution() const { return m_shadowResolution; }
    inline Color getColor() const { return m_color; }
    inline float getIntensity() const { return m_intensity; }
private:
    Color m_color;
    float m_intensity = 0;
    LightType m_type = LT_POINT;
    float m_range=0;

    float m_spotInnerConeAngle=0;
    float m_spotOuterConeAngle=0;

    bool m_enableShadows=true;
    int m_shadowResolution=1024;
};

