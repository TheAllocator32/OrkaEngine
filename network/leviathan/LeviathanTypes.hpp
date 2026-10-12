/*
 * File: LeviathanTypes.hpp
 *
 * Declares different Types used by Leviathan
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <vector>
#include <string>
#include "../../essentials/Identifier.hpp"

enum LeviathanDataChannel {
    LDC_GAMEPLAY = 0,                      // Gameplay line - used for miscelaneous nonsense and movement stuff
    LDC_EVENTS = 1,                        // Events line - used for important stuff that has to be tansfered
    LDC_HOST_CORRECTIONS_AND_EVENTS = 2,   // Host corrections and events line - (host only) used to send position and state updates 
};

enum LeviathanActionType {
    LAT_PLAYER,     // Automatically applies action to CharacterController
    LAT_WORLD,      // Automatically applies action to World/CharacterController of the World object (reliable channel recommended)
    LAT_CUSTOM      // Calls your selected callback
};

enum LeviathanConnectionStatus {
    LCS_DISCONNECTED,           // Not connected yet
    LCS_CONNECTION_PENDING,     // Connection in progress
    LCS_CONNECTED,              // Connected and encrypted
    LCS_ERROR                   // Something went completely wrong
};

enum LeviathanLobbyStatus {
    LLS_ERROR,      // Error :_(
    LLS_JOINABLE,   // You can join :)
    LLS_COUNTDOWN,  // Hurry up!
    LLS_GAME,       // Game in progress :(
};

struct LeviathanLobbyConfiguration {
    int maxPlayers = 20;
    int countdownStartPlayerCount = 20;
    float countdownDuration = 30.0f;
    bool allowManualStart = false;
    float manualStartVoteThresholdPercent = 100.0f;
};

struct LeviathanLobby {
    Identifier id = -1;                                 // ID/number
    int playersLoggedIn = 0;                            // Number of players currently logged in
    int maxPlayers = 0;                                 // Maximum number of players allowed in this lobby
    bool allowManualStart = false;                      // Whether connected players can vote to start the game
    float manualStartVoteThresholdPercent = 100.0f;     // Required percentage of lobby votes to start manually
    LeviathanLobbyStatus status = LLS_ERROR;            // Status
    int timeLeft = -1;                                  // Time left until game starts (only valid if LIS_COUNTDOWN)
};

struct VoteResult {
    std::unordered_map<std::string, std::string> votes; // string Player; string vote
    std::string voteResult;
};

struct MultiVoteResult {
    std::unordered_map<std::string, std::vector<std::string>> votes; // string Player; vector<string> vote
    std::string voteResult;
};

class OrkaPlayerBase; // Forward decl
struct LeviathanParticipant {
    Identifier participantID = -1; // Server-assigned, lobby-scoped ID
    std::string username;
    std::string verificationToken;
    OrkaPlayerBase* player;
};

