#if !defined(ORKA_DISABLE_NETWORKING) && !defined(ORKA_DISABLE_LEVIATHAN)
/*
 * File: LeviathanServer.cpp
 *
 * Implementation of Orka's Leviathan Server (Leviathan V1)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "LeviathanServer.hpp"
#include "../../essentials/HardwareJudger.hpp"
#include "../../OrkaVersion.h"
#include <string_view>
#include <span>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <cstdint>

#ifdef CP_NONE
#undef CP_NONE // Needed because of the damn Windows API
#endif

#if LEVIATHAN_LOBBY_SIZE > 30
#error Because of performance reasons, Leviathan V1 only supports up to 30 players per lobby!
#endif

/*
 * Channel purposes:
 * Channel 0: Inputs (All clients)                          - UNRELIABLE - Server Broadcasts & logs to file (compressed)
 *            Maximum 256 bytes and 20 packets per client per server update.
 * Channel 1: Events line                                   - RELIABLE   - Server Broadcasts & logs to file (compressed)
 *            Maximum 512 bytes and 5 packets per client per server update.
 * Channel 2: Host correction/events line                   - RELIABLE   - Server Broadcasts (only host allowed) & logs to file
 *            Maximum 512 KiB and 1 packet per client per server update.
 * Channel 3: PlayerID/Participant broadcast                - RELIABLE   - Server Broadcasts
 *            Maximum 1024 bytes and 1 packet per client per server update.
 * Channel 4: Lobby and match control                       - RELIABLE   - Server Processes
 * Channel 5: Version selection + Encryption handshake      - RELIABLE   - Server Processes
 */

/*
 * Server communication packets:
 * Encryption (channel 5):
 *    Version selection (client): REQVERSION + Leviathan version string
 *        Version selection answer (server): VERSIONOK
 *    Encryption start (client): STARTCRYPT + N/L/F (encryption profile) + client handshake blob
 *        Encryption answer (server): RESPCRYPT + server handshake blob
 *    Encryption finalize (client): ENDCRYPT
 *        Encryption finalize (server): ENDCRYPT
 * 
 * Lobby (Send and receive direction encrypted) (channel 4):
 *    Fetch Lobbies (client): REQLOBBIES
 *        Fetch Lobbies Answer (server): WhaleML
 *    Login Lobby (client): REQLOGIN + LobbyID
 *        Login Lobby Answer OK (server): LOGINOK + two-digit ParticipantID
 *        Login Lobby Answer FAILED (server): ERROR1/2/3 (1-Lobby does not exist 2-Lobby full 3-Game started)
 *    Send Hardware Judgement (client): HWJUDGEMENT + Hardware Score
 *        Send Hardware Judgement Answer ERROR (server): NLOGGEDIN (Not logged in into a lobby)
 *    Elect Host (server): YOUAREHOST
 *        Elect Host Answer (client): YEAH
 *    Unseat Host (server): YOUAREUNSEATED
 *        Unseat Host Answer (client): FINE
 *    Start Game (server): STARTGAME
 *        Start Game Answer (client): STARTED
 *    End Game (client, host only): ENDGAME
 *        End Game broadcast (server): ENDGAME
 *        End Game Answer (client): ENDED
 *    Abort Game (server): GAMEABORTED
 *        Abort Game Answer (client): ABORTED
 *    Kick Client (server): KICK
 *    Leave Lobby (client): LEAVE
 *        Leave Lobby Answer OK (server): OK
 *        Leave Lobby Answer ERROR (server): NLOGGEDIN
 * 
 * Miscelaneous:
 *    Relayed client packets (channels 0, 1 and 3, server): two-digit participantID + payload
 */

constexpr std::array<size_t, 4> xg_maximumRelayPacketSizes = { 256, 512, 512 * 1024, 1024 };
constexpr std::array<unsigned int, 4> xg_maximumRelayPacketsPerUpdate = { 20, 5, 1, 1 };
// The fullest crypto profile adds 40 bytes of nonce and authentication data.
constexpr std::array<size_t, 4> xg_maximumEncryptedRelayPacketSizes = {
    256 + 40,
    512 + 40,
    512 * 1024 + 40,
    1024 + 40
};

// Hardcode allowed versions
void LeviathanServer::allowVersions() {
    allowVersion(LEVIATHAN_VERSION_STRING); // Always
    allowVersion("LEVIATHAN_1.0@ORKA_2.1");
}

static LeviathanLobbyConfiguration normalizeLobbyConfiguration(LeviathanLobbyConfiguration configuration) {
    configuration.maxPlayers = std::clamp(configuration.maxPlayers, 1, LEVIATHAN_LOBBY_SIZE);
    configuration.countdownStartPlayerCount = std::clamp(
        configuration.countdownStartPlayerCount, 1, configuration.maxPlayers);
    configuration.countdownDuration = std::max(configuration.countdownDuration, 0.0f);
    configuration.manualStartVoteThresholdPercent = std::clamp(
        configuration.manualStartVoteThresholdPercent, 0.0f, 100.0f);
    return configuration;
}

void LeviathanServer::configureLobbies(std::vector<LeviathanLobbyConfiguration> configurations) {
    if (m_srv) {
        Logger::log(LET_WARN, "Orka, Leviathan Server", "Lobby configuration ignored after the server has started.");
        return;
    }
    if (configurations.empty()) {
        Logger::log(LET_WARN, "Orka, Leviathan Server", "Lobby configuration must contain at least one lobby.");
        return;
    }

    for (LeviathanLobbyConfiguration& configuration : configurations)
        configuration = normalizeLobbyConfiguration(configuration);
    m_lobbyConfigurations = std::move(configurations);
}

void LeviathanServer::configureLobbies(size_t lobbyCount, LeviathanLobbyConfiguration configuration) {
    configureLobbies(std::vector<LeviathanLobbyConfiguration>(lobbyCount, configuration));
}

bool LeviathanServer::configureLobby(size_t lobbyIndex, LeviathanLobbyConfiguration configuration) {
    if (m_srv) {
        Logger::log(LET_WARN, "Orka, Leviathan Server", "Lobby configuration ignored after the server has started.");
        return false;
    }
    if (lobbyIndex >= m_lobbyConfigurations.size()) {
        Logger::log(LET_WARN, "Orka, Leviathan Server", "Lobby index " + std::to_string(lobbyIndex) + " is outside the configured lobby range.");
        return false;
    }

    m_lobbyConfigurations[lobbyIndex] = normalizeLobbyConfiguration(configuration);
    return true;
}

bool LeviathanServer::configureLobbiesFromWhaleML(std::string_view filepath) {
    if (m_srv) {
        Logger::log(LET_WARN, "Orka, Leviathan Server", "Lobby configuration ignored after the server has started.");
        return false;
    }

    WhaleMLFile db(filepath);
    if (!db.isValid()) {
        Logger::log(LET_ERROR, "Orka, Leviathan Server", "Could not load lobby configuration from WhaleML file '" + std::string(filepath) + "'.");
        return false;
    }

    bool hasLobbyCount = false;
    const int lobbyCount = db.integer("lobbies.count", 0, hasLobbyCount);
    if (!hasLobbyCount || lobbyCount < 1) {
        Logger::log(LET_ERROR, "Orka, Leviathan Server", "WhaleML lobby configuration requires lobbies.count to be at least one.");
        return false;
    }

    auto readConfiguration = [&db](std::string_view prefix, LeviathanLobbyConfiguration configuration) {
        const std::string path(prefix);
        configuration.maxPlayers = db.integer(path + ".maxPlayers", configuration.maxPlayers);
        configuration.countdownStartPlayerCount = db.integer(path + ".countdownStartPlayerCount", configuration.countdownStartPlayerCount);
        configuration.countdownDuration = db.floating(path + ".countdownDuration", configuration.countdownDuration);
        configuration.allowManualStart = db.boolean(path + ".allowManualStart", configuration.allowManualStart);
        configuration.manualStartVoteThresholdPercent = db.floating(path + ".manualStartVoteThresholdPercent", configuration.manualStartVoteThresholdPercent);
        return configuration;
    };

    LeviathanLobbyConfiguration defaultConfiguration = readConfiguration("lobbies.default", {});
    std::vector<LeviathanLobbyConfiguration> configurations(
        static_cast<size_t>(lobbyCount), defaultConfiguration);
    for (int lobbyIndex = 0; lobbyIndex < lobbyCount; ++lobbyIndex) {
        const std::string prefix = "lobbies.lobby" + std::to_string(lobbyIndex);
        configurations[static_cast<size_t>(lobbyIndex)] = readConfiguration(
            prefix, defaultConfiguration);
    }

    configureLobbies(std::move(configurations));
    return true;
}

void LeviathanServer::start(int port) {
    if (m_lobbyConfigurations.empty()) {
        Logger::log(LET_CRITICAL_ERROR, "Orka, Leviathan Server", "Cannot start without lobby configurations. Call configureLobbies() first.");
        return;
    }

    m_numLobbies = static_cast<int>(m_lobbyConfigurations.size());
    if (m_logFilePath.empty()) m_logFilePath = "LeviathanServer.log";

    Logger::log(LET_INFO, "Orka, Leviathan Server", "Starting Leviathan Server version '" + std::string(LEVIATHAN_VERSION_STRING) + "' on port " + std::to_string(port) + " with " + std::to_string(m_numLobbies) + " lobbies.");

    // (Re-)judge the Hardware without ping and GPU judgement (because this is a server!)
    HardwareJudger::judgeAll(true, true, true);
    const HardwareCategory serverHardwareCategory = g_hardwareJudgement.hardwareCategory;
    HardwareJudger::printToTerminal();
    switch (serverHardwareCategory) {
    case HC_CALCULATOR:
        Logger::log(LET_CRITICAL_ERROR, "Orka Leviathan Server", "Your Hardware is not server-worthy!! Aborting...");
        return;
    case HC_EMBEDDED:
        Logger::log(m_numLobbies <= 2 ? LET_INFO : LET_WARN, "Orka, Leviathan Server", "Your Hardware is not meant to be a server! Limiting to 2 lobbies!");
        m_numLobbies = std::min(m_numLobbies, 2);
        break;
    case HC_POTATO:
        Logger::log(m_numLobbies <= 15 ? LET_INFO : LET_WARN, "Orka, Leviathan Server", "Your hardware is meh. Limiting to 15 lobbies!");
        m_numLobbies = std::min(m_numLobbies, 15);
        break;
    case HC_AVERAGE:
        Logger::log(m_numLobbies <= 30 ? LET_INFO : LET_WARN, "Orka, Leviathan Server", "Your Hardware is... fine... not more. Limiting to 30 lobbies.");
        m_numLobbies = std::min(m_numLobbies, 30);
        break;
    case HC_RICH:
        Logger::log(m_numLobbies <= 70 ? LET_INFO : LET_WARN, "Orka, Leviathan Server", "Your Hardware is pretty good! Still limiting to 70 lobbies...");
        m_numLobbies = std::min(m_numLobbies, 70);
        break;
    case HC_NASA_ADMIN:
        Logger::log(m_numLobbies <= 130 ? LET_SPECIAL : LET_WARN, "Orka, Leviathan Server", "Your Hardware IS really meant to be a server or workstation! Still limiting to 130 lobbies (which shouldn't be a limit at all...)");
        m_numLobbies = std::min(m_numLobbies, 130);
        break;
    case HC_DATA_CENTER:
        Logger::log(LET_SPECIAL, "Orka, Leviathan Server", "Your Hardware is completely overkill for a Leviathan server! Obviously no limits applied");
        break;
    default:
        Logger::log(LET_CRITICAL_ERROR, "Orka, Leviathan Server", "Hardware judgement returned no valid category. Aborting server startup.");
        return;
    }

    m_numLobbies = std::max(m_numLobbies, 1);

    m_srv = new RealtimeServer();
    m_srv->setOnConnect([this](ENetPeer* p) { onConnect(p); });
    m_srv->setOnMessage([this](ENetPeer* p, std::string_view m, uint8_t c) { onMessage(p, std::move(m), c); });
    m_srv->setOnDisconnect([this](ENetPeer* p) { onDisconnect(p); });

    allowVersions();
    std::string allowedVersionsMessage = "Allowed Leviathan client versions: ";
    int counter = 0;
    for (const auto& version : m_allowedVersions) {
        counter++;
        allowedVersionsMessage += version;
        if (counter != m_allowedVersions.size())
            allowedVersionsMessage += "; ";
    }
    Logger::log(LET_INFO, "Orka, Leviathan Server", allowedVersionsMessage);

    int maxPeers = 0;
    for (int lobbyId = 0; lobbyId < m_numLobbies; ++lobbyId)
        maxPeers += getLobbyConfiguration(lobbyId).maxPlayers;
    auto error = m_srv->start(port, maxPeers, 6); // Leviathan needs 6 channels (0-5)
}

void LeviathanServer::onConnect(ENetPeer* peer) {
    if (!peer) return;
    enet_peer_ping_interval(peer, LEVIATHAN_DEAD_PEER_PING_INTERVAL_MS);
    enet_peer_timeout(peer, LEVIATHAN_DEAD_PEER_TIMEOUT_LIMIT,
        LEVIATHAN_DEAD_PEER_TIMEOUT_MINIMUM_MS, LEVIATHAN_DEAD_PEER_TIMEOUT_MAXIMUM_MS);
}

void LeviathanServer::onDisconnect(ENetPeer* peer) {
    m_pendingInstructions.erase(peer);
    m_relayPacketsThisUpdate.erase(peer);

    // Drop crypto session
    m_cryptoSessions.erase(peer);
    m_peerVersions.erase(peer);

    // Drop hardware judgement
    m_hardwareJudgements.erase(peer);
    m_pendingHardwareJudgements.erase(peer);

    removePeerFromLobby(peer);
}

void LeviathanServer::handleMessage(ENetPeer* peer, std::string_view message, short channel) {
    // Encryption handshake
    if (channel == 5) { // Channel 5 is used for crypto handshake only
        std::string_view m = message;

        if (m.starts_with("REQVERSION")) {
            std::string version(m.substr(10));
            if (version.empty()) {
                m_srv->sendTo(peer, "ERROR5", 5, true); // Error 5 - Missing Leviathan version
                return;
            }
            if (!isVersionAllowed(version)) {
                m_srv->sendTo(peer, "ERROR6", 5, true); // Error 6 - Leviathan version rejected
                m_peerVersions.erase(peer);
                m_cryptoSessions.erase(peer);
                return;
            }

            m_peerVersions[peer] = version;
            m_srv->sendTo(peer, "VERSIONOK", 5, true);
            return;
        }

        if (m.starts_with("STARTCRYPT")) { // Start encryption negotiation
            if (!m_peerVersions.contains(peer)) {
                m_srv->sendTo(peer, "ERROR7", 5, true); // Error 7 - Version must be negotiated first
                return;
            }

            // "STARTCRYPT"(10) + mode(1) + at least 1 byte payload
            if (m.size() < 12) {
                m_srv->sendTo(peer, "ERROR1", 5, true); // Error 1 - Bad STARTCRYPT packet (too short)
                return;
            }

            char profile = m[10];
            if (profile == 'N') {        // No encryption
                m_cryptoSessions[peer] = std::make_unique<CryptoSession>("LEVIATHAN_CRYPTO_SESSION", CR_SERVER, CP_NONE);
            } else if (profile == 'L') {   // Light encryption
                m_cryptoSessions[peer] = std::make_unique<CryptoSession>("LEVIATHAN_CRYPTO_SESSION", CR_SERVER, CP_LIGHT);
            } else if (profile == 'F') {   // Full encryption
                m_cryptoSessions[peer] = std::make_unique<CryptoSession>("LEVIATHAN_CRYPTO_SESSION", CR_SERVER, CP_FULL);
            } else {
                // Client did not provide a valid encryption profile -> negotiation fails
                m_srv->sendTo(peer, "ERROR2", 5, true); // Error 2 - Unsupported encryption profile
                m_cryptoSessions.erase(peer);
                return;
            }

            auto* sess = m_cryptoSessions[peer].get();
            if (!sess) {
                m_srv->sendTo(peer, "ERROR2", 5, true);
                m_cryptoSessions.erase(peer);
                return;
            }

            std::string_view clientHello = m.substr(11);
            std::span<const uint8_t> sp(reinterpret_cast<const uint8_t*>(clientHello.data()), clientHello.size());
            std::vector<uint8_t> responseBytes = sess->handleClientHello(sp);
            std::string serverHello = "RESPCRYPT";
            serverHello.append(reinterpret_cast<const char*>(responseBytes.data()), responseBytes.size());
            m_srv->sendTo(peer, serverHello, 5, true);
            return;
        } else if (m.starts_with("ENDCRYPT")) {
            auto it = m_cryptoSessions.find(peer);
            if (it == m_cryptoSessions.end() || !it->second) {
                m_srv->sendTo(peer, "ERROR3", 5, true); // Error 3 - No active crypto session
                return;
            }
            it->second->finalizeServer();
            m_srv->sendTo(peer, "ENDCRYPT", 5, true); // Confirm finalization
            return;
        }

        // Unknown crypto control packet; silently ignore or send error
        m_srv->sendTo(peer, "ERROR4", 5, true); // Unknown crypto command
        return;
    }

    // All other channels must be encrypted / decrypted
    auto csIt = m_cryptoSessions.find(peer);
    if (csIt == m_cryptoSessions.end() || !csIt->second) {
        m_srv->sendTo(peer, "ENCRYPTFIRST", 5, true);
        return;
    }
    CryptoSession* c = csIt->second.get();

    if (channel >= 0 && channel < 4) {
        if (message.size() > xg_maximumEncryptedRelayPacketSizes[static_cast<size_t>(channel)]) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Dropped oversized encrypted relay packet on channel " + std::to_string(channel) + ".");
            return;
        }
    }

    std::string decrypted = c->decryptString(message);
    std::string_view m = decrypted;

    if (channel == 4) { // Channel 4 is used for lobby and match control.
        if (m == "YEAH" || m == "FINE" || m == "STARTED" || m == "ENDED" || m == "ABORTED") {
            auto pendingIt = m_pendingInstructions.find(peer);
            if (pendingIt != m_pendingInstructions.end()) {
                auto& pending = pendingIt->second;
                std::erase_if(pending, [&m](const PendingInstruction& instruction) {
                    return instruction.acknowledgement == m;
                });
                if (pending.empty()) m_pendingInstructions.erase(pendingIt);
            }
            return;
        }

        if (m.starts_with("REQLOGIN")) { // Client wants to login!
            int lobbyID = 0;
            try {
                lobbyID = std::stoi(std::string(m.substr(8)));
            }
            catch (...) {
                m_srv->sendTo(peer, c->encryptString("ERROR1"), 4, true); // Malformed lobby ID
                return;
            }

            if (lobbyID < 0 || lobbyID >= m_numLobbies) { // Lobby does not exist
                m_srv->sendTo(peer, c->encryptString("ERROR1"), 4, true); // Error 1 = Lobby does not exist
                return;
            }
            const LeviathanLobbyConfiguration& configuration = getLobbyConfiguration(lobbyID);
            if (m_lobbies[lobbyID].size() >= static_cast<size_t>(configuration.maxPlayers)) { // Lobby full
                m_srv->sendTo(peer, c->encryptString("ERROR2"), 4, true); // Error 2 = Lobby full
                return;
            }
            if (m_games[lobbyID]) { // Game pending / started
                m_srv->sendTo(peer, c->encryptString("ERROR3"), 4, true); // Error 3 = Game started
                return;
            }

            bool participantIdsInUse[100] {};
            for (ENetPeer* lobbyPeer : m_lobbies[lobbyID]) {
                if (auto participantIdIt = m_peerParticipantIds.find(lobbyPeer);
                    participantIdIt != m_peerParticipantIds.end()) {
                    participantIdsInUse[participantIdIt->second] = true;
                }
            }
            unsigned int participantId = 0;
            while (participantId < 100 && participantIdsInUse[participantId]) ++participantId;
            if (participantId == 100) {
                Logger::log(LET_ERROR, "Orka Leviathan Server", "Unable to assign a two-digit participant ID to a lobby client.");
                m_srv->sendTo(peer, c->encryptString("ERROR2"), 4, true);
                return;
            }

            m_lobbies[lobbyID].insert(peer);
            m_peerLobby[peer] = lobbyID;
            m_peerParticipantIds[peer] = participantId;
            m_pendingHardwareJudgements[peer] = LEVIATHAN_HARDWARE_JUDGEMENT_TIMEOUT_SECONDS;

            // LOGINOK must be the first lobby-control packet. The client uses it to
            // finish its login request before it can process host instructions.
            const std::string loginOk = "LOGINOK" + std::to_string(participantId / 10) + std::to_string(participantId % 10);
            m_srv->sendTo(peer, c->encryptString(loginOk), 4, true);
            return;
        }

        if (m.starts_with("REQLOBBIES")) { // Client wants an overview of our lobbies!
            WhaleMLFile db; // WhaleML mode
            std::vector<std::string> allowedVersions(m_allowedVersions.begin(), m_allowedVersions.end());
            std::sort(allowedVersions.begin(), allowedVersions.end());
            db.setValues("leviathan.versions", allowedVersions);
            db.setValue("lobbies.count", m_numLobbies);
            for (int i = 0; i < m_numLobbies; i++) {
                const LeviathanLobbyConfiguration& configuration = getLobbyConfiguration(i);
                db.navigate("lobby." + std::to_string(i));
                db.setValue("numPlayers", static_cast<int>(m_lobbies[i].size()));
                db.setValue("maxPlayers", configuration.maxPlayers);
                db.setValue("allowManualStart", configuration.allowManualStart);
                db.setValue("manualStartVoteThresholdPercent", configuration.manualStartVoteThresholdPercent);
                db.setValue("open", !m_games[i]);
                db.setValue("countdown", m_lobbyCountdowns.contains(i));
            }
            m_srv->sendTo(peer, c->encryptString(db.dumpSingleLine()), 4, true);
            return;
        }

        if (m.starts_with("HWJUDGEMENT")) {
            if (!m_peerLobby.contains(peer)) {
                m_srv->sendTo(peer, c->encryptString("NLOGGEDIN"), 4, true);
                return;
            }

            int score = 0;
            try {
                score = std::stoi(std::string(m.substr(11)));
            }
            catch (...) {
                m_srv->sendTo(peer, c->encryptString("ERROR4"), 4, true); // Bad score
                return;
            }

            if (!m_pendingHardwareJudgements.erase(peer)) {
                Logger::log(LET_WARN, "Orka Leviathan Server", "Ignoring repeated hardware judgement from client.");
                return;
            }

            m_hardwareJudgements[peer] = score;
            const int lobbyID = m_peerLobby[peer];
            if (hasAllLobbyHardwareJudgements(lobbyID)) {
                electLobbyHost(lobbyID);
                if (m_lobbies[lobbyID].size() >= static_cast<size_t>(getLobbyConfiguration(lobbyID).countdownStartPlayerCount))
                    startLobbyCountdown(lobbyID);
            }

            return;
        }

        if (m == "VOTESTARTGAME") {
            auto peerLobbyIt = m_peerLobby.find(peer);
            if (peerLobbyIt == m_peerLobby.end()) {
                m_srv->sendTo(peer, c->encryptString("NLOGGEDIN"), 4, true);
                return;
            }

            const int lobbyId = peerLobbyIt->second;
            const LeviathanLobbyConfiguration& configuration = getLobbyConfiguration(lobbyId);
            if (!configuration.allowManualStart || m_games[lobbyId]) return;

            auto& votes = m_manualStartVotes[lobbyId];
            votes.insert(peer);
            const size_t participants = m_lobbies[lobbyId].size();
            if (participants > 0 &&
                static_cast<float>(votes.size()) * 100.0f >=
                static_cast<float>(participants) * configuration.manualStartVoteThresholdPercent) {
                startLobbyGame(lobbyId);
            }
            return;
        }

        if (m == "ENDGAME") {
            auto peerLobbyIt = m_peerLobby.find(peer);
            if (peerLobbyIt == m_peerLobby.end()) {
                m_srv->sendTo(peer, c->encryptString("NLOGGEDIN"), 4, true);
                return;
            }

            const int lobbyId = peerLobbyIt->second;
            if (!m_games[lobbyId] || m_lobbyHost[lobbyId] != peer) {
                Logger::log(LET_WARN, "Orka Leviathan Server", "Blocked ENDGAME from a peer that is not the host of an active game.");
                return;
            }

            m_games.erase(lobbyId);
            m_lobbyCountdowns.erase(lobbyId);
            m_lobbyCountdownBroadcasts.erase(lobbyId);
            m_manualStartVotes.erase(lobbyId);
            for (ENetPeer* lobbyPeer : m_lobbies[lobbyId]) {
                sendInstruction(lobbyPeer, "ENDGAME", "ENDED");
            }
            return;
        }

        if (m.starts_with("LEAVE")) {
            auto plIt = m_peerLobby.find(peer);
            if (plIt == m_peerLobby.end()) {
                m_srv->sendTo(peer, c->encryptString("NLOGGEDIN"), 4, true);
                return;
            }

            removePeerFromLobby(peer);
            m_hardwareJudgements.erase(peer);

            m_srv->sendTo(peer, c->encryptString("OK"), 4, true);
            return;
        }

        return;
    }

    // Broadcast channels 0-3. Channel 2 is host-only, so it needs no sender ID.
    if (channel >= 0 && channel < 4) {
        auto plIt = m_peerLobby.find(peer);
        if (plIt == m_peerLobby.end()) return;

        auto participantIdIt = m_peerParticipantIds.find(peer);
        if (participantIdIt == m_peerParticipantIds.end()) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Blocked relay packet from a peer without a participant ID.");
            return;
        }

        int lobbyId = plIt->second;
        auto lobIt = m_lobbies.find(lobbyId);
        if (lobIt == m_lobbies.end()) return;

        if (m.size() > xg_maximumRelayPacketSizes[static_cast<size_t>(channel)]) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Dropped oversized relay packet on channel " + std::to_string(channel) + " in lobby " + std::to_string(lobbyId) + ".");
            return;
        }
        unsigned int& packetsThisUpdate = m_relayPacketsThisUpdate[peer][static_cast<size_t>(channel)];
        if (packetsThisUpdate >= xg_maximumRelayPacketsPerUpdate[static_cast<size_t>(channel)]) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Dropped rate-limited relay packet on channel " + std::to_string(channel) + " in lobby " + std::to_string(lobbyId) + ".");
            return;
        }
        ++packetsThisUpdate;

        if (channel == 2 && m_lobbyHost[lobbyId] != peer) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Blocked non-host packet on channel 2 in lobby " + std::to_string(lobbyId));
            logPacketToFile(peer, lobbyId, channel, "BLOCKED_NON_HOST " + std::string(m));
            return;
        }

        if (channel <= 2) logPacketToFile(peer, lobbyId, channel, m);

        const bool prependParticipantId = channel == 0 || channel == 1 || channel == 3;
        if (prependParticipantId && m.size() >= 2 && m[0] >= '0' && m[0] <= '9' && m[1] >= '0' && m[1] <= '9') {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Relayed packet starts with two digits and may be attempting to spoof a participant ID.");
        }

        std::string relayedMessage(m);
        if (prependParticipantId) {
            const unsigned int participantId = participantIdIt->second;
            relayedMessage.reserve(m.size() + 2);
            relayedMessage.insert(relayedMessage.begin(), {
                static_cast<char>('0' + participantId / 10),
                static_cast<char>('0' + participantId % 10)
            });
        }

        for (ENetPeer* p : lobIt->second) {
            if (p == peer) continue;
            auto csTargetIt = m_cryptoSessions.find(p);
            if (csTargetIt == m_cryptoSessions.end() || !csTargetIt->second) continue;

            CryptoSession* s = csTargetIt->second.get();
            m_srv->sendTo(p, s->encryptString(relayedMessage), channel, channel > 0); // Channels 1-3 reliable, 0 unreliable
        }
    }
}

void LeviathanServer::update(float delta) {
    if (m_srv) {
        m_relayPacketsThisUpdate.clear();
        m_srv->update(delta);
        updateLobbyCountdowns(delta);
        updatePendingHardwareJudgements(delta);
        updatePendingInstructions(delta);
    }
}

void LeviathanServer::updatePendingHardwareJudgements(float delta) {
    std::vector<ENetPeer*> peersToKick;
    for (auto& [peer, remaining] : m_pendingHardwareJudgements) {
        remaining -= std::max(0.0f, delta);
        if (remaining <= 0.0f) {
            Logger::log(LET_WARN, "Orka Leviathan Server", "Kicking client because it did not provide a hardware judgement within " + std::to_string(LEVIATHAN_HARDWARE_JUDGEMENT_TIMEOUT_SECONDS) + " seconds.");
            peersToKick.push_back(peer);
        }
    }

    for (ENetPeer* peer : peersToKick)
        kickPeer(peer);
}

void LeviathanServer::sendInstruction(ENetPeer* peer, std::string_view instruction, std::string_view acknowledgement) {
    auto cryptoIt = m_cryptoSessions.find(peer);
    if (!m_srv || cryptoIt == m_cryptoSessions.end() || !cryptoIt->second) return;

    m_srv->sendTo(peer, cryptoIt->second->encryptString(instruction), 4, true);
    auto& pending = m_pendingInstructions[peer];
    std::erase_if(pending, [&instruction, &acknowledgement](const PendingInstruction& existing) {
        return existing.instruction == instruction && existing.acknowledgement == acknowledgement;
    });
    pending.push_back({
        .instruction = std::string(instruction),
        .acknowledgement = std::string(acknowledgement),
        .retryIn = 1.0f,
        .sends = 1
    });
}

void LeviathanServer::updatePendingInstructions(float delta) {
    std::vector<ENetPeer*> peersToKick;
    for (auto& [peer, instructions] : m_pendingInstructions) {
        for (PendingInstruction& pending : instructions) {
            pending.retryIn -= std::max(0.0f, delta);
            if (pending.retryIn > 0.0f) continue;

            if (pending.sends >= 3) {
                Logger::log(LET_WARN, "Orka Leviathan Server", "Kicking client after it did not acknowledge instruction '" + pending.instruction + "' after 3 sends.");
                peersToKick.push_back(peer);
                break;
            }

            auto cryptoIt = m_cryptoSessions.find(peer);
            if (cryptoIt == m_cryptoSessions.end() || !cryptoIt->second) {
                peersToKick.push_back(peer);
                break;
            }

            m_srv->sendTo(peer, cryptoIt->second->encryptString(pending.instruction), 4, true);
            ++pending.sends;
            pending.retryIn = 1.0f;
        }
    }

    std::sort(peersToKick.begin(), peersToKick.end());
    peersToKick.erase(std::unique(peersToKick.begin(), peersToKick.end()), peersToKick.end());
    for (ENetPeer* peer : peersToKick)
        kickPeer(peer);
}

void LeviathanServer::kickPeer(ENetPeer* peer) {
    auto cryptoIt = m_cryptoSessions.find(peer);
    const bool kickQueued = m_srv && cryptoIt != m_cryptoSessions.end() && cryptoIt->second &&
        m_srv->sendTo(peer, cryptoIt->second->encryptString("KICK"), 4, true) == NE_SUCCESS;

    removePeerFromLobby(peer);
    if (!m_srv) return;
    if (kickQueued)
        m_srv->disconnectLater(peer);
    else
        m_srv->disconnect(peer);
}

void LeviathanServer::removePeerFromLobby(ENetPeer* peer) {
    m_pendingInstructions.erase(peer);
    m_pendingHardwareJudgements.erase(peer);
    auto peerLobbyIt = m_peerLobby.find(peer);
    if (peerLobbyIt == m_peerLobby.end()) return;

    const int lobbyId = peerLobbyIt->second;
    if (auto hostIt = m_lobbyHost.find(lobbyId); hostIt != m_lobbyHost.end() && hostIt->second == peer)
        hostIt->second = nullptr;
    if (auto votesIt = m_manualStartVotes.find(lobbyId); votesIt != m_manualStartVotes.end())
        votesIt->second.erase(peer);

    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt != m_lobbies.end()) {
        lobbyIt->second.erase(peer);
        const bool gameIsActive = m_games.contains(lobbyId) && m_games.at(lobbyId);

        // A running game needs at least two connected participants. Otherwise a
        // departing player could leave its lobby permanently marked as in-game.
        if (gameIsActive && lobbyIt->second.size() < 2) {
            m_games.erase(lobbyId);
            m_lobbyCountdowns.erase(lobbyId);
            m_lobbyCountdownBroadcasts.erase(lobbyId);
            m_manualStartVotes.erase(lobbyId);
            for (ENetPeer* remainingPeer : lobbyIt->second) {
                sendInstruction(remainingPeer, "GAMEABORTED", "ABORTED");
            }
        }

        if (lobbyIt->second.size() < static_cast<size_t>(getLobbyConfiguration(lobbyId).countdownStartPlayerCount)) {
            m_lobbyCountdowns.erase(lobbyId);
            m_lobbyCountdownBroadcasts.erase(lobbyId);
        }
        if (lobbyIt->second.empty()) {
            m_lobbies.erase(lobbyIt);
            m_lobbyHost.erase(lobbyId);
            m_games.erase(lobbyId);
            m_manualStartVotes.erase(lobbyId);
        }
    }

    m_peerLobby.erase(peerLobbyIt);
    m_peerParticipantIds.erase(peer);
    if (hasAllLobbyHardwareJudgements(lobbyId))
        electLobbyHost(lobbyId);
}

void LeviathanServer::startLobbyCountdown(int lobbyId) {
    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt == m_lobbies.end() || lobbyIt->second.size() < 2 ||
        m_games[lobbyId] || m_lobbyCountdowns.contains(lobbyId)) return;

    const float countdownDuration = getLobbyConfiguration(lobbyId).countdownDuration;
    m_lobbyCountdowns[lobbyId] = countdownDuration;
    const int secondsRemaining = static_cast<int>(std::ceil(countdownDuration));
    m_lobbyCountdownBroadcasts[lobbyId] = secondsRemaining;
    broadcastCountdown(lobbyId, secondsRemaining);
}

void LeviathanServer::updateLobbyCountdowns(float delta) {
    for (auto it = m_lobbyCountdowns.begin(); it != m_lobbyCountdowns.end();) {
        const int lobbyId = it->first;
        it->second = std::max(0.0f, it->second - std::max(0.0f, delta));
        const int secondsRemaining = static_cast<int>(std::ceil(it->second));
        if (m_lobbyCountdownBroadcasts[lobbyId] != secondsRemaining) {
            m_lobbyCountdownBroadcasts[lobbyId] = secondsRemaining;
            broadcastCountdown(lobbyId, secondsRemaining);
        }

        if (it->second > 0.0f) {
            ++it;
            continue;
        }

        it = m_lobbyCountdowns.erase(it);
        startLobbyGame(lobbyId);
    }
}

void LeviathanServer::electLobbyHost(int lobbyId) {
    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt == m_lobbies.end() || lobbyIt->second.empty()) return;

    ENetPeer* selectedHost = nullptr;
    int highestScore = -20000;
    for (ENetPeer* candidate : lobbyIt->second) {
        const auto scoreIt = m_hardwareJudgements.find(candidate);
        const int score = scoreIt != m_hardwareJudgements.end() ? scoreIt->second : -20000;
        if (!selectedHost || score > highestScore) {
            selectedHost = candidate;
            highestScore = score;
        }
    }

    ENetPeer*& currentHost = m_lobbyHost[lobbyId];
    if (currentHost == selectedHost) return;
    if (currentHost) {
        sendInstruction(currentHost, "YOUAREUNSEATED", "FINE");
    }

    currentHost = selectedHost;
    sendInstruction(selectedHost, "YOUAREHOST", "YEAH");
}

bool LeviathanServer::hasAllLobbyHardwareJudgements(int lobbyId) const {
    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt == m_lobbies.end() || lobbyIt->second.empty()) return false;
    for (ENetPeer* peer : lobbyIt->second) {
        if (m_pendingHardwareJudgements.contains(peer)) return false;
    }
    return true;
}

void LeviathanServer::startLobbyGame(int lobbyId) {
    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt == m_lobbies.end() || lobbyIt->second.size() < 2 || m_games[lobbyId]) return;

    m_games[lobbyId] = true;
    m_lobbyCountdowns.erase(lobbyId);
    m_lobbyCountdownBroadcasts.erase(lobbyId);
    m_manualStartVotes.erase(lobbyId);

    for (ENetPeer* peer : lobbyIt->second) {
        sendInstruction(peer, "STARTGAME", "STARTED");
    }
}

const LeviathanLobbyConfiguration& LeviathanServer::getLobbyConfiguration(int lobbyId) const {
    return m_lobbyConfigurations.at(static_cast<size_t>(lobbyId));
}

void LeviathanServer::broadcastCountdown(int lobbyId, int secondsRemaining) {
    auto lobbyIt = m_lobbies.find(lobbyId);
    if (lobbyIt == m_lobbies.end()) return;

    const std::string message = "COUNTDOWN" + std::to_string(secondsRemaining);
    for (ENetPeer* peer : lobbyIt->second) {
        auto cryptoIt = m_cryptoSessions.find(peer);
        if (cryptoIt == m_cryptoSessions.end() || !cryptoIt->second) continue;
        m_srv->sendTo(peer, cryptoIt->second->encryptString(message), 4, true);
    }
}

void LeviathanServer::setLogFilePath(std::string_view path) {
    m_logFilePath = std::string(path);
}

void LeviathanServer::logPacketToFile(ENetPeer* peer, int lobbyId, short channel, std::string_view payload) {
    if (m_logFilePath.empty()) return;

    std::lock_guard<std::mutex> guard(m_logMutex);
    std::error_code ec;
    std::filesystem::path logPath(m_logFilePath);
    if (logPath.has_parent_path()) std::filesystem::create_directories(logPath.parent_path(), ec);

    std::ofstream file(m_logFilePath, std::ios::app);
    if (!file.is_open()) {
        Logger::log(LET_WARN, "Orka Leviathan Server", "Could not open Leviathan log file '" + m_logFilePath + "'");
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << " lobby=" << lobbyId
        << " channel=" << channel
        << " peer=" << reinterpret_cast<std::uintptr_t>(peer)
        << " payload=" << sanitizeLogPayload(payload)
        << '\n';
}

bool LeviathanServer::isVersionAllowed(std::string_view version) const {
    return m_allowedVersions.contains(std::string(version));
}

std::string LeviathanServer::sanitizeLogPayload(std::string_view payload) {
    std::string sanitized;
    sanitized.reserve(payload.size());
    for (char ch : payload) {
        switch (ch) {
        case '\n':
            sanitized += "\\n";
            break;
        case '\r':
            sanitized += "\\r";
            break;
        case '\t':
            sanitized += "\\t";
            break;
        default:
            sanitized += ch;
            break;
        }
    }
    return sanitized;
}

#endif

