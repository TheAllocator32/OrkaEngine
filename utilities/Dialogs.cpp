/*
 * File: Dialogs.cpp
 *
 * Contains some dialogs and utilities
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Tools.hpp"
#if !defined(ORKA_DISABLE_GUI) && !defined(ORKA_DISABLE_NETWORKING)
#include "../gui/internal_widgets/AllWidgets.hpp"
#include "../network/Network.hpp"
#include <memory>

void Dialogs::downloadFileWithWidget(std::string_view URL, std::string_view destination) {
    Identifier id = GUIManager::addWidget<FileLoadingWidget>();
    auto& w = static_cast<FileLoadingWidget&>(GUIManager::getWidget(id));
    w.setFileName(destination);
    w.setID(id);


    Network::downloadFile(
        URL,
        destination,
        [id](float p) {
                auto& w = static_cast<FileLoadingWidget&>(GUIManager::getWidget(id));
                w.setProgress(p);
        },
        [id](bool status) {
                auto& w = static_cast<FileLoadingWidget&>(GUIManager::getWidget(id));
                w.setStatus(true, status);
        }
    );
}
#endif

