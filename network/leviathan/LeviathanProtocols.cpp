#if !defined(ORKA_DISABLE_NETWORKING) && !defined(ORKA_DISABLE_LEVIATHAN)
/*
 * File: LeviathanProtocols.cpp
 *
 * Leviathan protocol message handling
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Leviathan.hpp"
#include "../../essentials/Logger.hpp"
#include "../../whaleml/WhaleMLFile.hpp"
#include <charconv>

/*
 * Leviathan Channel 1 and 2 Packets
 *  Entering lobby:
 *   - Me, Channel 3: IAMNEW + WhaleML-singleline containing player.username and player.verification
 *   - Other Clients, Channel 3: HELLO + WhaleML-singleline containing player.username and player.verification
 *  Vote:
 *   - Channel 1: VOTE + WhaleML-singleline containing vote.subject and vote.value
 *
 *
 */

void LeviathanSession::receiveNetwork(std::string_view data, short channel) {
    if (!m_crypto) return;
    const std::string decryptedMessage = m_crypto->decryptString(data);
    std::string_view message = decryptedMessage;
    int senderParticipantID = -1;
    if (channel == 0 || channel == 1 || channel == 3) {
        if (message.size() < 2 || message[0] < '0' || message[0] > '9' || message[1] < '0' || message[1] > '9') {
            Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Received relayed packet without a two-digit participant ID.");
            return;
        }
        senderParticipantID = (message[0] - '0') * 10 + (message[1] - '0');
        message.remove_prefix(2);
    }

    if (channel == 1) {
        if (message.contains("VOTE")) {

        }
    } else if (channel == 3) {
        WhaleMLFile receivedCredentials;
        if (message.contains("IAMNEW")) {
            receivedCredentials.loadBuffer(message.substr(6));
        } else if (message.contains("HELLO")) {
            receivedCredentials.loadBuffer(message.substr(5));
        }

        if (receivedCredentials.isValid()) {
            receivedCredentials.navigate("player");
            const std::string username = receivedCredentials.string("username");
            const std::string verificationToken = receivedCredentials.string("verification");
            if (!receivedCredentials.getValue("username").isValid() || username.empty()) {
                Logger::log(LET_WARN, "Orka, Leviathan Netcode", "WhaleML buffer did not contain required login credentials!");
                return;
            }
            if (username != m_username) {
                // If a client tries to login with multiple usernames, remove all old entries to avoid clients with multiple identities!
                std::erase_if(m_participants, [&](const LeviathanParticipant& p) {
                    return p.participantID == senderParticipantID ||
                           p.username == username;
                });
                LeviathanParticipant p;
                p.participantID = senderParticipantID;
                p.username = username;
                p.verificationToken = verificationToken;
                m_participants.push_back(p);
                // Now tell the new player who you are
                sendHello(false);
            }
        } else {
            Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Expected WhaleML buffer containing player credentials... Received nonsense instead");
        }
    } else if (channel == 4) {
        if (message.starts_with("COUNTDOWN")) {
            int seconds = -1;
            const std::string_view value = message.substr(9);
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), seconds);
            if (error == std::errc{} && end == value.data() + value.size() && seconds >= 0)
                m_lobbyCountdownSeconds = seconds;
            else
                Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Received a malformed lobby countdown.");
        } else if (message == "YOUAREHOST") {
            m_sessionIsHost = true;
            m_network->send(m_crypto->encryptString("YEAH"), 4, true);
        } else if (message == "YOUAREUNSEATED") {
            m_sessionIsHost = false;
            m_network->send(m_crypto->encryptString("FINE"), 4, true);
        } else if (message == "STARTGAME") {
            m_gameRunning = true;
            m_lobbyCountdownSeconds = -1;
            m_network->send(m_crypto->encryptString("STARTED"), 4, true);
        } else if (message == "ENDGAME") {
            m_gameRunning = false;
            m_lobbyCountdownSeconds = -1;
            m_network->send(m_crypto->encryptString("ENDED"), 4, true);
        } else if (message == "GAMEABORTED") {
            Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Server aborted current match. Returning to lobby...");
            m_gameRunning = false;
            m_lobbyCountdownSeconds = -1;
            m_network->send(m_crypto->encryptString("ABORTED"), 4, true);
        } else if (message == "KICK") {
            Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server kicked us from the lobby!");
            m_gameRunning = false;
            m_sessionIsHost = false;
            m_lobbyCountdownSeconds = -1;
            m_connectedLobby = -1;
            m_participantID = -1;
            m_participants.clear();
            m_voteResults.clear();
            m_multiVoteResults.clear();
            m_network->destroy();
            m_connectionStatus = LCS_ERROR;
        }
    }
}

bool LeviathanSession::sendHello(bool iamnew) {
    if (!m_crypto || !m_network) return false;
    if (m_username.empty()) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Unable to connect to send user information. No username provided! Call setCredentials before connecting to a lobby next time. Leaving lobby...");
        leave();
        return false;
    }

    WhaleMLFile payload;
    payload.setValue("player.username", m_username);
    if (m_verificationTokenGenerator) payload.setValue("player.verification", m_verificationTokenGenerator());

    std::string messageStart;
    if (iamnew) messageStart = "IAMNEW";
    else messageStart = "HELLO";
    m_network->send(m_crypto->encryptString(messageStart + payload.dumpSingleLine()), 3, true);
    return true;
}

void LeviathanSession::voteStartGame() const {
    if (m_connectionStatus != LCS_CONNECTED || !m_network || !m_crypto || m_connectedLobby == -1) {
        Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Cannot vote to start a game before the Leviathan session is connected to a lobby!");
        return;
    }
    m_network->send(m_crypto->encryptString("VOTESTARTGAME"), 4, true);
}

void LeviathanSession::vote(std::string_view subject, std::string_view value) {
    if (m_connectionStatus != LCS_CONNECTED || !m_network || !m_crypto || m_connectedLobby == -1) {
        Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Cannot vote on '" + std::string(subject) + "'! Session is not connected to a lobby!");
        return;
    }
    WhaleMLFile f;
    f.setValue("vote.subject", subject);
    f.setValue("vote.value", value);
    m_network->send(m_crypto->encryptString("VOTE" + f.dumpSingleLine()), 4, true);
}

// -- Host only --

void LeviathanSession::endGame() {
    if (m_connectionStatus != LCS_CONNECTED || !m_network || !m_crypto || m_connectedLobby == -1) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Could not end the game because this session is not connected to a lobby!");
        return;
    }
    if (!m_sessionIsHost) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Only the host may end the game");
        return;
    }
    if (!m_gameRunning) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Could not end the game because there is no game currently running!");
        return;
    }
    m_network->send(m_crypto->encryptString("ENDGAME"), 4, true);
}

void LeviathanSession::sendSceneHostCorrection(std::span<Identifier> exclude) {
    WhaleMLFile db;


}

#endif

