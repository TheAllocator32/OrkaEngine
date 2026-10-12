/*
 * File: AllWidgets.hpp
 *
 * Contains widgets like FileDownloadDialog and ErrorDialog
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../GUIManager.hpp"
#include "../../primitives/gui/GUITypes.hpp"

class FileLoadingWidget : public GUIWidget {
public:
    void setProgress(float progress);
    void setStatus(bool finished, bool success);
    void setFileName(std::string_view filename);
    inline void load() override {}
    void draw(float delta) override;
    inline void unload() override {};
private:
    float m_progress = 0.0f;
    bool m_isFinished = false, m_isOK = false;
    std::string m_filename;
};

class ProgressWidget : public GUIWidget {
public:
    inline void load() override {}
    void draw(float delta) override;
    inline void unload() override {}
};

class RenderingConfigurationWidget : public GUIWidget {
public:
    inline void load() override {}
    void draw(float delta) override;
    inline void unload() override {}
};

