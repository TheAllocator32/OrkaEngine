#ifndef ORKA_DISABLE_GUI
/*
 * File: FileLoadingWidget.cpp
 *
 * A simple File downloading dialog!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "AllWidgets.hpp"
#include "../../utilities/Tools.hpp"

using namespace ImGuiTools;

void FileLoadingWidget::setProgress(float progress) {
    m_progress = progress;
}

void FileLoadingWidget::setStatus(bool finished, bool OK) {
    m_isFinished = finished; m_isOK = OK;
}

void FileLoadingWidget::setFileName(std::string_view filename) {
    m_filename = filename;
}

void FileLoadingWidget::draw(float delta) {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = p2ToVec(30, 20);
    ImVec2 centeredPos = ImVec2(
        (displaySize.x - windowSize.x) * 0.5f,
        (displaySize.y - windowSize.y) * 0.5f
    );

    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowPos(centeredPos);

    ImGui::PushFont(GUIManager::getFont("ArialMedium"));
    if (ImGui::Begin(_("Download"))) {
        ImGui::PushFont(GUIManager::getFont("ArialLarge"));
        ImGui::TextUnformatted(_("Downloading file..."));
        ImGui::PopFont();
        ImGui::PushFont(GUIManager::getFont("ArialMedium"));

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(m_filename.c_str());
            ImGui::EndTooltip();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (!m_isFinished) {
            ImSpinner::SpinnerAng("##download_spinner", 10.0f, 2.5f,
                                  ImColor(90, 190, 255), ImColor(90, 190, 255, 48), 2.6f);
            ImGui::SameLine();
            ImGui::TextDisabled(m_progress < 0.0f ? "Connecting and downloading..." : "Downloading...");

            if (m_progress < 0.0f) {
                ImGui::ProgressBar(-1.0f, ImVec2(-1.0f, 0.0f), _("Preparing transfer..."));
            } else {
                std::string label = std::to_string(static_cast<int>(m_progress)) + "%";
                ImGui::ProgressBar(m_progress / 100.0f, ImVec2(-1.0f, 0.0f), _(label.c_str()));
            }
        } else {
            if (m_isOK) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", _("Download complete."));
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", _("Download failed!"));
            }

            ImGui::Spacing();
            if (ImGui::Button(_("OK"), ImVec2(100, 0))) {
                GUIManager::removeWidget(m_id);
            }
        }
        ImGui::PopFont();
    }
    ImGui::PopFont();
    ImGui::End();
}


#endif

