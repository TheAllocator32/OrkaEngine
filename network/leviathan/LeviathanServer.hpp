/*
 * File: LeviathanServer.hpp
 *
 * The LeviathanServer module is the reference implementation of Orka Leviathan's relay server
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../../crypto/Crypto.hpp"
#include "../NetworkGameplay.hpp"
#include "LeviathanTypes.hpp"
#include "../../whaleml/WhaleMLFile.hpp"
#include "../../essentials/Logger.hpp"
#include "../../OrkaVersion.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <array>

#define LEVIATHAN_LOBBY_SIZE 30 // Defines the maximum number of Players in a lobby (Please don't set above 30 because of Performance reasons!)
#define LEVIATHAN_DEAD_PEER_PING_INTERVAL_MS 2000
#define LEVIATHAN_DEAD_PEER_TIMEOUT_LIMIT 3
#define LEVIATHAN_DEAD_PEER_TIMEOUT_MINIMUM_MS 5000
#define LEVIATHAN_DEAD_PEER_TIMEOUT_MAXIMUM_MS 15000
#define LEVIATHAN_HARDWARE_JUDGEMENT_TIMEOUT_SECONDS 5

class LeviathanServer {
public:
    // Configures all lobbies. Call before start(); the array length defines the lobby count.
    void configureLobbies(std::vector<LeviathanLobbyConfiguration> configurations);
    // Configures lobbyCount identical lobbies using the supplied configuration template.
    void configureLobbies(size_t lobbyCount, LeviathanLobbyConfiguration configuration = {});
    // Replaces one lobby configuration. Call after configureLobbies() and before start().
    bool configureLobby(size_t lobbyIndex, LeviathanLobbyConfiguration configuration);
    // Loads lobbies.count, lobbies.default.*, and lobbies.lobbyN.* settings from a WhaleML file.
    bool configureLobbiesFromWhaleML(std::string_view filepath);
    // Starts the Server
    void start(int port=12522);
    // Updates the Server (has to be called e.g. in a seperate thread every millisecond because the whole gameplay depends on the relay speed!)
    void update(float delta);
    
    // Functions you should never call
    void onConnect(ENetPeer* peer);
    inline void onMessage(ENetPeer* peer, std::string_view message, short channel) { try { handleMessage(peer, message, channel); } catch (std::exception e) { Logger::log(LET_WARN, "Orka Leviathan Server", "Exception while processing packet '" + std::string(message) + "'! Proceeding anyway"); } }
    void onDisconnect(ENetPeer* peer);

    // Internals
    void handleMessage(ENetPeer* peer, std::string_view message, short channel);

    void setLogFilePath(std::string_view path);
private:
    // Control functions
    void allowVersions();
    inline void allowVersion(std::string_view version) { if (!version.empty()) m_allowedVersions.insert(std::string(version)); }
    inline void clearAllowedVersions() { m_allowedVersions.clear(); }
    static std::string sanitizeLogPayload(std::string_view payload);
    void logPacketToFile(ENetPeer* peer, int lobbyId, short channel, std::string_view payload);
    bool isVersionAllowed(std::string_view version) const;
    void removePeerFromLobby(ENetPeer* peer);
    void startLobbyCountdown(int lobbyId);
    void updateLobbyCountdowns(float delta);
    void updatePendingHardwareJudgements(float delta);
    void updatePendingInstructions(float delta);
    void sendInstruction(ENetPeer* peer, std::string_view instruction, std::string_view acknowledgement);
    void kickPeer(ENetPeer* peer);
    void broadcastCountdown(int lobbyId, int secondsRemaining);
    void electLobbyHost(int lobbyId);
    bool hasAllLobbyHardwareJudgements(int lobbyId) const;
    void startLobbyGame(int lobbyId);
    const LeviathanLobbyConfiguration& getLobbyConfiguration(int lobbyId) const;
    std::string getDefaultLogFilePath() const;

    RealtimeServer* m_srv = nullptr;
    std::unordered_map<ENetPeer*, std::unique_ptr<CryptoSession>> m_cryptoSessions;
    std::unordered_map<ENetPeer*, std::string> m_peerVersions;
    std::unordered_map<int, std::unordered_set<ENetPeer*>> m_lobbies;
    std::unordered_map<ENetPeer*, int> m_peerLobby; // Reversed m_lobbies
    // Two-digit, lobby-scoped ID added by the server to relayed packets.
    std::unordered_map<ENetPeer*, unsigned int> m_peerParticipantIds;
    std::unordered_map<int, ENetPeer*> m_lobbyHost;
    std::unordered_map<ENetPeer*, int> m_hardwareJudgements;
    std::unordered_map<ENetPeer*, float> m_pendingHardwareJudgements;
    std::unordered_map<int, bool> m_games;
    std::unordered_map<int, float> m_lobbyCountdowns;
    std::unordered_map<int, int> m_lobbyCountdownBroadcasts;
    std::unordered_map<int, std::unordered_set<ENetPeer*>> m_manualStartVotes;
    // Cleared at the beginning of each server update; indexed by relay channel 0-3.
    std::unordered_map<ENetPeer*, std::array<unsigned int, 4>> m_relayPacketsThisUpdate;
    struct PendingInstruction {
        std::string instruction;
        std::string acknowledgement;
        float retryIn = 1.0f;
        int sends = 1;
    };
    std::unordered_map<ENetPeer*, std::vector<PendingInstruction>> m_pendingInstructions;
    std::unordered_set<std::string> m_allowedVersions;
    std::vector<LeviathanLobbyConfiguration> m_lobbyConfigurations;
    std::string m_logFilePath;
    mutable std::mutex m_logMutex;
    int m_tickTime = 0;
    int m_numLobbies = 0;
};

