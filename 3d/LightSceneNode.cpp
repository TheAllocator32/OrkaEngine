/*
 * File: ModelSceneNode.hpp
 *
 * A SceneNode containing a light source. Can be described using .light.whaleml
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "LightSceneNode.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "essentials/Logger.hpp"

void LightSceneNode::loadFile(std::string_view file) {
    WhaleMLFile db(file);
    db.navigate("light");

    std::string name = db.string("name", "!!EMPTY_NAME!!");
    LightType type = LT_POINT;
    Color color;
    float intensity = db.floating("intensity", 1);
    float range;
    degrees spotInnerConeAngle = 0;
    degrees spotOuterConeAngle = 0;

    auto colorList = db.getValues("color");
    if (colorList.size() != 3) {
        Logger::log(LET_WARN, "Orka, LightSceneNode", "Light '" + std::string(name) + "' is missing the 'color' field (or color field is malformed)!");
        return;
    } if (colorList[0].asInt() < 0 || colorList[1].asInt() < 0 || colorList[2].asInt() < 0) {
        Logger::log(LET_WARN, "Orka, LightSceneNode", "Light '" + std::string(name) + "' has a damn NEGATIVE color... Try to imagine that");
        return;
    } if (colorList[0].asInt() > 1 || colorList[1].asInt() > 1 || colorList[2].asInt() > 1) { // Interpret as 8bit int
        color.set(colorList[0].asInt(), colorList[1].asInt(), colorList[2].asInt());
    } else { // Interpret as float 0-1
        color.set(colorList[0].asFloat(), colorList[1].asFloat(), colorList[2].asFloat());
    }

    if (db.hasKey("range"))
        range = db.floating("range");
    else {
        Logger::log(LET_WARN, "Orka, LightSceneNode", "Light '" + std::string(name) + "' is missing the 'range' field!");
        return;
    } if (range <= 0) {
        Logger::log(LET_WARN, "Orka, LightSceneNode", "Light '" + std::string(name) + "' has invalid an invalid range value (either 0 or negative)!");
        return;
    }

    if (db.hasKey("type")) {
        if (db["type"].asString() == "spot" || db["type"].asString()=="SPOT" || db["type"].asString()=="Spot") { // Point light
            type = LT_SPOT;
            db.navigate("spot");
            if (db.hasKey("inner_cone"))
                spotInnerConeAngle = db.floating("inner_cone");
            else {
                Logger::log(LET_WARN, "Orka, LightSceneNode", "Spotlight '" + std::string(name) + "' is missing the 'spot.inner_cone' field!");
                return;
            }
            if (db.hasKey("outer_cone"))
                spotOuterConeAngle = db.floating("outer_cone");
            else
                spotOuterConeAngle = spotInnerConeAngle;

            if (spotInnerConeAngle <= 0|| spotOuterConeAngle <= 0) {
                Logger::log(LET_WARN, "Orka, LightSceneNode", "Spotlight '" + std::string(name) + "' has invalid cone value(s) (either 0 or negative)!");
                return;
            }
        }
    } else {
        Logger::log(LET_WARN, "Orka, LightSceneNode", "Light '" + std::string(name) + "' is missing the 'type' field!");
        return;
    }

    db.navigate("shadow");
    bool enableShadows = db.boolean("enable", false);
    int shadowResolution = db.integer("resolution", 1024);

    db.navigate("script");
    std::string scriptPath = db.string("file", "");
    bool instanceScript = db.boolean("instance", true);

    createManual(name, type, color, intensity, range, scriptPath, instanceScript, spotOuterConeAngle, spotInnerConeAngle, enableShadows, shadowResolution);
}

void LightSceneNode::createManual(std::string_view name, LightType type, Color color, float intensity, float range, std::string_view scriptPath, bool instanceScript, float outerCone, float innerCone, bool enableShadows, int shadowResolution) {
    setName(name);

    m_light.setLightType(type);
    m_light.setColor(color);
    m_light.setIntensity(intensity);
    m_light.setRange(range);

    m_light.setSpotOuterConeAngle(outerCone);
    m_light.setSpotInnerConeAngle(innerCone);

    m_light.setEnableShadows(enableShadows);
    m_light.setShadowResolution(shadowResolution);

    SceneNode::create(scriptPath, instanceScript);
}

LightSceneNode& LightSceneNode::clone() const {
    LightSceneNode& newNode = CreatableSceneNode::clone();
    newNode.m_light = m_light;
    return newNode;
}