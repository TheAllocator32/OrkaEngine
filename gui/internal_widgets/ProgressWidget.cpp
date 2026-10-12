#ifndef ORKA_DISABLE_GUI
/*
 * File: ProgressWidget.cpp
 *
 * Shows current ProgressManager state in an ImGui widget.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "AllWidgets.hpp"
#include "../../essentials/ProgressManager.hpp"

void ProgressWidget::draw(float) {
    const Progress progress = ProgressManager::getProgress();
    const float fraction = progress.total > 0
        ? static_cast<float>(progress.current) / static_cast<float>(progress.total)
        : 0.0f;

    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Loading Progress", nullptr, ImGuiWindowFlags_NoCollapse)) {
        const bool hasActiveWork = !progress.currentElement.name.empty() ||
            (!progress.currentElement.currentMessage.empty() && progress.currentElement.currentMessage != "waiting");
        if (hasActiveWork) {
            ImSpinner::SpinnerAng("##progress_spinner", 9.0f, 2.5f,
                                  ImColor(100, 220, 255), ImColor(100, 220, 255, 48), 2.6f);
            ImGui::SameLine();
            ImGui::TextUnformatted("Loading");
        } else {
            ImGui::TextDisabled("Waiting for work");
        }

        ImGui::Text("Overall: %d / %d", progress.current, progress.total);
        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));

        if (!progress.currentElement.name.empty()) {
            ImGui::Text("Current: %s", progress.currentElement.name.c_str());
        } else {
            ImGui::TextUnformatted("Current: idle");
        }

        ImGui::Text("Element progress: %d / %d",
                    progress.currentElement.currentLocal,
                    progress.currentElement.totalLocal);

        const float currentElementFraction = progress.currentElement.totalLocal > 0
            ? static_cast<float>(progress.currentElement.currentLocal) / static_cast<float>(progress.currentElement.totalLocal)
            : 0.0f;
        ImGui::ProgressBar(currentElementFraction, ImVec2(-1.0f, 0.0f));

        if (!progress.currentElement.currentMessage.empty()) {
            ImGui::Text("Step: %s", progress.currentElement.currentMessage.c_str());
        } else {
            ImGui::TextUnformatted("Step: waiting");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Terminal");

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 14, 18, 230));
        ImGui::BeginChild("progress_terminal", ImVec2(0.0f, 110.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 160, 255));

        if (progress.elements.empty()) {
            ImGui::TextUnformatted("> waiting for progress events...");
        } else {
            for (const auto& [id, element] : progress.elements) {
                const std::string stepText = element.currentMessage.empty() ? "waiting" : element.currentMessage;
                ImGui::TextWrapped("> [%d] %s: %s (%d/%d)",
                                   id.getValue(),
                                   element.name.c_str(),
                                   stepText.c_str(),
                                   element.currentLocal,
                                   element.totalLocal);
            }

            if (!progress.currentMessages.empty()) {
                ImGui::Separator();
                const std::size_t visibleLines = 6;
                const std::size_t startIndex = progress.currentMessages.size() > visibleLines
                    ? progress.currentMessages.size() - visibleLines
                    : 0;

                for (std::size_t i = startIndex; i < progress.currentMessages.size(); ++i) {
                    const std::string& message = progress.currentMessages[i];
                    ImGui::TextWrapped("  %s", message.c_str());
                }
            }

            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

#endif

