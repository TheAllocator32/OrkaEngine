#ifndef ORKA_DISABLE_AUDIO
/*
 * File: MusicManager.cpp
 *
 * This file contains the implementation of the .music.whaleml parser
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "MusicManager.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "../primitives/audio/Sound.hpp"
#include <filesystem>
#include "essentials/Logger.hpp"

struct MusicEntry {
    std::string file;
    bool loop = false;
    float loopPoint = -1.0f;
    float gain = 1.0f;
    std::string name;
    std::string artist;
    std::string date;
};

static MusicEntry readEntry(WhaleMLFile& db, std::string_view prefix) {
    const std::string root = prefix.empty() ? "" : (std::string(prefix) + ".");
    MusicEntry entry;
    entry.file = db.string(root + "file", "");
    entry.loop = db.boolean(root + "loop", false);
    entry.loopPoint = db.floating("loop.point", -1.0f);
    entry.gain = db.floating(root + "gain", 1.0f);
    entry.name = db.string("meta.name", "");
    entry.artist = db.string("meta.artists", "");
    entry.date = db.string("meta.date", "");
    return entry;
}

static std::string buildAudioPath(std::string_view folder, std::string_view file) {
    std::filesystem::path filePath{std::string(file)};
    if (filePath.is_absolute()) {
        return filePath.string();
    }
    if (folder.empty() || folder == ".") {
        return filePath.string();
    }
    return (std::filesystem::path(std::string(folder)) / filePath).lexically_normal().string();
}

static MusicProperties applyEntry(const MusicEntry& entry, std::string_view folder, std::string_view id) {
    Sound& sound = Sound::create();
    sound.load(buildAudioPath(folder, entry.file));
    sound.setLoop(entry.loop);
    if (entry.loopPoint >= 0.0f) sound.setLoopPoint(entry.loopPoint);
    sound.setVolume(entry.gain);

    MusicProperties prop;
    prop.name = entry.name;
    prop.artist = entry.artist;
    prop.date = entry.date;
    prop.id = id;
    prop.soundID = sound.getID();
    return prop;
}

void MusicManager::processFile(std::string_view path, std::string_view id) {
    Sound* loadedSound = getSound(id);
    if (loadedSound == nullptr || !loadedSound->isLoaded()) {
        WhaleMLFile db;
        db.load(path);
        const std::string idString(id);

        std::filesystem::path fsPath{std::string(path)};
        std::string folder = fsPath.parent_path().string();

        MusicEntry audio = readEntry(db, "audio");
        if (!audio.file.empty()) {
            sm_music[idString] = applyEntry(audio, folder, id);
            return;
        }

        Logger::log(LET_WARN, "Orka, Music File Parser", "File '" + std::string(path) + "' is no valid .music.whaleml file! Check syntax!");
    }
}

std::string_view MusicManager::getName(std::string_view id) {
    auto it = sm_music.find(std::string(id));
    if (it == sm_music.end()) {
        return {};
    }
    return it->second.name;
}

std::string_view MusicManager::getArtist(std::string_view id) {
    auto it = sm_music.find(std::string(id));
    if (it == sm_music.end()) {
        return {};
    }
    return it->second.artist;
}

Sound* MusicManager::getSound(std::string_view id) {
    auto it = sm_music.find(std::string(id));
    if (it == sm_music.end() || it->second.soundID == -1) {
        return nullptr;
    }
    return AudioEngine::findSound(it->second.soundID);
}

/*
void MusicManager::showMusicDialog(std::string_view id) {
    MusicFileDialog* dialog = new MusicFileDialog();
    dialog->setMusic(sm_music[id]);
    GUIManager::addWidget(dialog);
}
*/

#endif

