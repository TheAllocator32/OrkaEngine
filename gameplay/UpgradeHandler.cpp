#ifndef ORKA_DISABLE_GAMEPLAY
/*
 * File: UpgradeHandler.cpp
 *
 * Implements the Upgrade System (dependency to CharacterController)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "UpgradeHandler.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include <sstream>
#include <algorithm>

static std::string normalizeValue(const WhaleMLScalarValue& value) {
    std::string out = value.asString();
    if (out.size() >= 2) {
        const bool doubleQuoted = out.front() == '"' && out.back() == '"';
        const bool singleQuoted = out.front() == '\'' && out.back() == '\'';
        if (doubleQuoted || singleQuoted) {
            out = out.substr(1, out.size() - 2);
        }
    }
    return out;
}

static int readIntOrDefault(WhaleMLFile& db, std::string_view key, int fallback) {
    return db.integer(key, fallback);
}

static std::string readStringOrDefault(WhaleMLFile& db, std::string_view key, std::string_view fallback = "") {
    return db.string(key, fallback);
}

static std::vector<int> splitUpgradeSteps(std::string_view s) {
    std::vector<int> result;
    std::stringstream ss{std::string(s)};
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (!item.empty())
            result.push_back(std::stoi(item));
    }
    return result;
}

static std::vector<int> fillUpgradeSteps(const std::vector<int>& baseSteps, int levels) {
    if (levels <= 0) {
        return {};
    }

    std::vector<int> out;
    out.reserve(levels);
    if (baseSteps.empty()) {
        out.assign(levels, 0);
        return out;
    }

    for (int i = 0; i < levels; ++i) {
        if (i < static_cast<int>(baseSteps.size()))
            out.push_back(baseSteps[i]);
        else
            out.push_back(baseSteps.back());
    }
    return out;
}

void UpgradeHandler::load(std::string_view path) {
    WhaleMLFile db;
    db.load(path);

    auto toStrings = [](const std::vector<WhaleMLScalarValue>& values) {
        std::vector<std::string> out;
        out.reserve(values.size());
        for (const WhaleMLScalarValue& v : values) {
            if (!v.isValid()) {
                out.push_back("");
                continue;
            }
            try {
                out.push_back(normalizeValue(v));
            } catch (...) {
                out.push_back("");
            }
        }
        return out;
    };

    auto parseInt = [](std::string_view value, int fallback) {
        try {
            return std::stoi(std::string(value));
        } catch (...) {
            return fallback;
        }
    };

    auto loadKeyedUpgrades = [&](std::string_view root) {
        const std::string rootString(root);
        std::vector<std::string> upgradeIds = db.getChildren(rootString);
        int loaded = 0;
        for (std::string_view upgradeId : upgradeIds) {
            if (upgradeId.empty()) {
                continue;
            }
            const std::string upgradeIdString(upgradeId);

            Upgrade u;
            u.id = upgradeIdString;
            u.func = readStringOrDefault(db, rootString + "." + upgradeIdString + ".func");
            if (u.func.empty()) {
                continue;
            }
            u.name = readStringOrDefault(db, rootString + "." + upgradeIdString + ".name");
            u.description = readStringOrDefault(db, rootString + "." + upgradeIdString + ".description");
            u.maxLevel = readIntOrDefault(db, rootString + "." + upgradeIdString + ".levels", 0);

            std::vector<std::string> listSteps = toStrings(db.getValues(rootString + "." + upgradeIdString + ".steps"));
            if (listSteps.empty()) {
                listSteps = toStrings(db.getValues(rootString + "." + upgradeIdString + ".upgrade_steps"));
            }
            if (!listSteps.empty()) {
                std::vector<int> parsedSteps;
                parsedSteps.reserve(listSteps.size());
                for (std::string_view s : listSteps) {
                    parsedSteps.push_back(parseInt(s, 0));
                }
                u.steps = fillUpgradeSteps(parsedSteps, u.maxLevel);
            } else {
                std::string semicolonSteps = readStringOrDefault(db, rootString + "." + upgradeIdString + ".upgrade_steps");
                if (semicolonSteps.empty()) {
                    semicolonSteps = readStringOrDefault(db, rootString + "." + upgradeIdString + ".steps");
                }
                u.steps = fillUpgradeSteps(splitUpgradeSteps(semicolonSteps), u.maxLevel);
            }

            m_upgrades.push_back(u);
            ++loaded;
        }
        return loaded;
    };

    int loadedUpgrades = 0;
    for (std::string_view root : {"upgrades", "Upgrades", "upgrade"}) {
        loadedUpgrades += loadKeyedUpgrades(root);
        if (loadedUpgrades > 0) break;
    }

    auto loadIndexedUpgrades = [&](std::string_view root, int startIndex) {
        int loaded = 0;
        for (int i = startIndex;; ++i) {
            const std::string indexedRoot = std::string(root) + "." + std::to_string(i);
            WhaleMLScalarValue id = db[indexedRoot + ".id"];
            WhaleMLScalarValue func = db[indexedRoot + ".func"];

            if (!id.isValid() && !func.isValid()) {
                break;
            }

            Upgrade u;
            u.id = readStringOrDefault(db, indexedRoot + ".id");
            u.description = readStringOrDefault(db, indexedRoot + ".description");
            u.func = readStringOrDefault(db, indexedRoot + ".func");
            u.name = readStringOrDefault(db, indexedRoot + ".name");
            u.maxLevel = readIntOrDefault(db, indexedRoot + ".levels", 0);
            u.steps = fillUpgradeSteps(splitUpgradeSteps(readStringOrDefault(db, indexedRoot + ".upgrade_steps")), u.maxLevel);
            m_upgrades.push_back(u);
            ++loaded;
        }
        return loaded;
    };

    if (loadedUpgrades == 0) {
        for (std::string_view root : {"upgrades", "Upgrades", "upgrades.upgrade", "Upgrades.upgrade", "upgrade"}) {
            loadedUpgrades += loadIndexedUpgrades(root, 0);
            if (loadedUpgrades > 0) break;
        }
    }
    if (loadedUpgrades == 0) {
        for (std::string_view root : {"upgrades", "Upgrades", "upgrades.upgrade", "Upgrades.upgrade", "upgrade"}) {
            loadedUpgrades += loadIndexedUpgrades(root, 1);
            if (loadedUpgrades > 0) break;
        }
    }

    if (loadedUpgrades == 0) {
        Logger::log(LET_WARN, "Orka, Upgrade Handler", "No upgrades loaded from WhaleML file: " + std::string(path));
    }
}

void UpgradeHandler::apply(std::string_view id, int level) {
    auto it = std::find_if(m_upgrades.begin(), m_upgrades.end(), [id](const Upgrade& e) {return e.id == id; });
    if (it == m_upgrades.end()) { Logger::log(LET_WARN, "Upgrade Handler, apply", "Upgrade '" + std::string(id) + "' not found!"); return;}
    std::string funcname = "void " + it->func + "(int)";
    int idx = std::min(level, it->maxLevel) - 1;
    if (idx < 0) idx = 0;
    int value = it->steps.at(idx);
    m_controller->invokeFunction(funcname, value);
}

void UpgradeHandler::apply(std::vector<std::string> ids, std::vector<int> levels) {
    for (size_t i = 0; i < ids.size() && i < levels.size(); ++i) {
        apply(ids[i], levels[i]);
    }
}

#endif

