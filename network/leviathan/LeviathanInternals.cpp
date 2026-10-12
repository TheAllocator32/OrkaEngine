#if !defined(ORKA_DISABLE_NETWORKING) && !defined(ORKA_DISABLE_LEVIATHAN)
/*
 * File: LeviathanInternals.cpp
 *
 * Internal connection and session implementation for Leviathan
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include <chrono>
#include <thread>
#include "Leviathan.hpp"
#include "../../essentials/HardwareJudger.hpp"
#include "../../essentials/Logger.hpp"
#include "../../whaleml/WhaleMLFile.hpp"

void LeviathanSession::receiveNetworkEncrypt(std::string_view data, short channel) {
    if (data.starts_with("VERSIONOK")) {
        auto blobBytes = m_crypto->beginClientHello();
        const char cryptoProfile = m_crypto->profile() == CP_FULL ? 'F' : m_crypto->profile() == CP_LIGHT ? 'L' : 'N';
        std::string clientHello = "STARTCRYPT";
        clientHello += cryptoProfile;
        clientHello.append(reinterpret_cast<const char*>(blobBytes.data()), blobBytes.size());
        m_network->send(clientHello, 5, true);
    }

    if (data.starts_with("RESPCRYPT")) {
        std::span<const uint8_t> sp(reinterpret_cast<const uint8_t*>(data.data() + 9), data.size() - 9);
        if (!m_crypto->finishClientHandshake(sp)) { Logger::log(LET_CRITICAL_ERROR, "Orka, Leviathan Netcode", "Could not encrypt data channel! Finish Client Handshake failed!"); exit(1); }
        m_network->send("ENDCRYPT", 5, true);
    }

    if (data.starts_with("ENDCRYPT")) {
        if (m_crypto->isReady()) {
            m_connectionStatus = LCS_CONNECTED;
            m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetwork(args, callbackChannel); });
        } else {
            Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Server is cheating on us! It tries to finalize crypto but we aren't ready!");
            m_connectionStatus = LCS_ERROR;
        }
    }

    if (data.starts_with("ERROR1")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server complains about our encryption initialization packet! It's too short!");
        m_connectionStatus = LCS_ERROR;
    }

    if (data.starts_with("ERROR2")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server complains about our encryption initialization packet! Bad crypto profile!");
        m_connectionStatus = LCS_ERROR;
    }

    if (data.starts_with("ERROR3")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server ghosted us! It thinks we don't exist :/");
        m_connectionStatus = LCS_ERROR;
    }

    if (data.starts_with("ERROR5")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server rejected our Leviathan version packet because it was malformed.");
        m_connectionStatus = LCS_ERROR;
    }

    if (data.starts_with("ERROR6")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server rejected our Leviathan version. We are too old or too new :/");
        m_connectionStatus = LCS_ERROR;
    }

    if (data.starts_with("ERROR7")) {
        Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server requires Leviathan version negotiation before crypto setup!");
        m_connectionStatus = LCS_ERROR;
    }
}

void LeviathanSession::reset() {
    delete m_network;
    delete m_crypto;
    m_crypto = nullptr;
    m_network = nullptr;
	m_gameRunning = false;
	m_sessionIsHost = false;
	m_lobbyCountdownSeconds = -1;
	m_connectedLobby = -1;
    m_participantID = -1;
    m_participants.clear();
    m_voteResults.clear();
    m_multiVoteResults.clear();
    if (m_connectionStatus != LCS_ERROR)
        m_connectionStatus = LCS_DISCONNECTED;
}

void LeviathanSession::connect(std::string_view address, uint16_t port) {
    Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Leviathan session connecting to server '" + std::string(address) + "' on port " + std::to_string(port));
    m_connectionStatus = LCS_CONNECTION_PENDING;
    m_network = new RealtimeClient();
    auto error = m_network->connect(address, port, 6);
    if (error != NE_SUCCESS) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Could not connect to server! Usually means server is unreachable from your machine or you chose a wrong address/port!");
        m_connectionStatus = LCS_ERROR;
        reset();
        return;
    }

    m_serverAddress = address;

    // Start Crypto
    if (!HardwareJudger::hasJudged())
        HardwareJudger::judgeAll(true, true, true);
    char cryptoSessionType = 'F';
    auto cat = g_hardwareJudgement.hardwareCategory;
    if (cat == HC_EMBEDDED || cat == HC_CALCULATOR) cryptoSessionType = 'L';

    m_crypto = new CryptoSession("LEVIATHAN_CRYPTO_SESSION", CR_CLIENT, cryptoSessionType=='F' ? CP_FULL : CP_LIGHT);
    m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetworkEncrypt(args, callbackChannel); });
    error = m_network->send(std::string("REQVERSION") + LEVIATHAN_VERSION_STRING, 5, true);
    if (error != NE_SUCCESS) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Failed to send initial negotiation request! We don't really know why :/");
        m_connectionStatus = LCS_ERROR;
        reset();
        return;
    }
    Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Connected successfully :)");
}

void LeviathanSession::disconnect() {
    Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Disconnecting from server...");
    if (m_network) m_network->destroy();
    reset();
    m_connectionStatus = LCS_DISCONNECTED;
}

bool LeviathanSession::connectToLobby(int lobbyID) {
    if (m_connectedLobby != -1) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Unable to connect to lobby " + std::to_string(lobbyID) + "! Session already connected to lobby " + std::to_string(m_connectedLobby) + "! Disconnect before connecting to a new lobby!");
        return false;
    }

    if (m_connectionStatus != LCS_CONNECTED) {
        Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Unable to connect to lobby " + std::to_string(lobbyID) + "! (Server not connected yet)");
        m_connectedLobby = -1;
        return false;
    }

	bool waitingForLoginResponse = true;
	bool error = true;

	m_network->setCallback([this, &waitingForLoginResponse, &error, lobbyID](std::string_view data, short callbackChannel) {
		if (callbackChannel != 4) return;
		auto decData = m_crypto->decryptString(data);

		if (decData.starts_with("LOGINOK")) {
			if (decData.size() != 9 || decData[7] < '0' || decData[7] > '9' || decData[8] < '0' || decData[8] > '9') {
				Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Server returned a malformed participant ID while joining lobby " + std::to_string(lobbyID) + "! Damn!");
				waitingForLoginResponse = false;
				error = true;
			} else {
			waitingForLoginResponse = false;
			m_connectedLobby = lobbyID;
			m_participantID = (decData[7] - '0') * 10 + (decData[8] - '0');
			m_gameRunning = false;
			error = false;
			}
		} else if (decData == "ERROR1") {
			Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Unable to connect to lobby " + std::to_string(lobbyID) + "! Lobby does not exist...");
			waitingForLoginResponse = false;
			error = true;
		} else if (decData == "ERROR2") {
			Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Unable to connect to lobby " + std::to_string(lobbyID) + "! Lobby is full :(");
			waitingForLoginResponse = false;
			error = true;
		} else if (decData == "ERROR3") {
			Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Unable to connect to lobby " + std::to_string(lobbyID) + "! Game already started :/");
			waitingForLoginResponse = false;
			error = true;
		} else {
			receiveNetwork(data, callbackChannel);
			return;
		}

		m_network->setCallback([this](std::string_view args, short receiveChannel) { receiveNetwork(args, receiveChannel); });
	});

	if (m_network->send(m_crypto->encryptString("REQLOGIN" + std::to_string(lobbyID)), 4, true) != NE_SUCCESS) {
		m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetwork(args, callbackChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Failed to send the lobby login request.");
		return false;
	}
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(LEVIATHAN_RESPONSE_TIMEOUT_SECONDS);
	while (waitingForLoginResponse && std::chrono::steady_clock::now() < deadline) {
		m_network->update();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	if (waitingForLoginResponse) {
		m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetwork(args, callbackChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Timed out waiting for the lobby login response.");
		m_connectedLobby = -1;
		return false;
	}

    bool judgementHadNetwork = false;
    bool judgementHadGPU = false;
    if (!HardwareJudger::hasJudged(judgementHadNetwork, judgementHadGPU) || !judgementHadNetwork || !judgementHadGPU) {
        Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Lobby host selection requires GPU and network results. Running a full hardware (re)judgement...");
        HardwareJudger::setGameServerAddress(m_serverAddress);
        HardwareJudger::judgeAll(true);
    }
    if (!HardwareJudger::hasPrintedToTerminal()) HardwareJudger::printToTerminal();

	if (!error && m_network->send(m_crypto->encryptString("HWJUDGEMENT" + std::to_string(static_cast<int>(g_hardwareJudgement.finalScore))), 4, true) != NE_SUCCESS) {
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Failed to send the Hardware Judgement. Leaving lobby...");
		error = true;
	}

	if (!error) error = !sendHello(true);
	if (error) {
		m_connectedLobby = -1;
		m_participantID = -1;
	}

    if (!error) Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Session connected to lobby " + std::to_string(lobbyID));
	return !error;
}

void LeviathanSession::update() {
    if (m_network) m_network->update();
}

void LeviathanSession::leave() {
	if (m_connectedLobby == -1) {
		Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Unable to leave lobby because this session is not connected to any lobby yet!");
	}

	bool waitingForResponse = true;

	m_network->setCallback([this, &waitingForResponse](std::string_view data, short callbackChannel) {
		if (callbackChannel != 4) return;
		auto decData = m_crypto->decryptString(data);

		if (decData == "NLOGGEDIN") {
			Logger::log(LET_BLOCK, "Orka, Leviathan Netcode", "Unable to leave lobby " + std::to_string(m_connectedLobby) + "! Server thinks we aren't connected to any lobby...");
		    m_connectedLobby = -1;
		    m_participantID = -1;
		    m_gameRunning = false;
		    m_sessionIsHost = false;
            m_lobbyCountdownSeconds = -1;
            m_participants.clear();
            m_voteResults.clear();
            m_multiVoteResults.clear();
		} else if (decData == "OK") {
		    Logger::log(LET_INFO, "Orka, Leviathan Netcode", "Left lobby " + std::to_string(m_connectedLobby));
			m_connectedLobby = -1;
			m_participantID = -1;
			m_gameRunning = false;
			m_sessionIsHost = false;
            m_lobbyCountdownSeconds = -1;
            m_participants.clear();
            m_voteResults.clear();
            m_multiVoteResults.clear();
		} else {
			receiveNetwork(data, callbackChannel);
			return;
		}

		m_network->setCallback([this](std::string_view args, short receiveChannel) { receiveNetwork(args, receiveChannel); });
		waitingForResponse = false;
	});

	if (m_network->send(m_crypto->encryptString("LEAVE"), 4, true) != NE_SUCCESS) {
		m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetwork(args, callbackChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Failed to send the leave-lobby request.");
		return;
	}

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(LEVIATHAN_RESPONSE_TIMEOUT_SECONDS);
	while (waitingForResponse && std::chrono::steady_clock::now() < deadline) {
		m_network->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
	if (waitingForResponse) {
		m_network->setCallback([this](std::string_view args, short callbackChannel) { receiveNetwork(args, callbackChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Timed out waiting for the leave-lobby response.");
	}
}

std::vector<LeviathanLobby> LeviathanSession::fetchLobbies() {
    if (m_connectionStatus != LCS_CONNECTED) {
		Logger::log(LET_WARN, "Orka, Leviathan Netcode", "Unable to fetch lobbies (Server not connected yet)");
        return std::vector<LeviathanLobby>();
    }

	bool waitingForResponse = true;
	std::vector<LeviathanLobby> lobbies;

	m_network->setCallback([this, &waitingForResponse, &lobbies](std::string_view data, short callbackChannel) {
		if (callbackChannel != 4) return;
		WhaleMLFile f;
		f.loadBuffer(m_crypto ? m_crypto->decryptString(data) : std::string(data));

		if (!f.isValid()) {
			receiveNetwork(data, callbackChannel);
			return;
		}

		for (const auto& lobby : f.getChildren("lobby")) {
			f.navigate("lobby." + lobby);
			LeviathanLobby l;
			try {
				l.id = std::stoi(lobby);
			}
			catch (...) {
				continue;
			}
			l.playersLoggedIn = f.integer("numPlayers", -1);
			l.maxPlayers = f.integer("maxPlayers", f.integer("maxPlayerCount", 0));
			l.allowManualStart = f.boolean("allowManualStart", false);
			l.manualStartVoteThresholdPercent = f.floating("manualStartVoteThresholdPercent", 100.0f);
			if (f.boolean("countdown", false))
				l.status = LLS_COUNTDOWN;
			else if (f.boolean("open", false))
				l.status = LLS_JOINABLE;
			else
				l.status = LLS_GAME;
			lobbies.push_back(l);
		}
		m_network->setCallback([this](std::string_view args, short receiveChannel) { receiveNetwork(args, receiveChannel); });
		waitingForResponse = false;
	});
	if (m_network->send(m_crypto->encryptString("REQLOBBIES"), 4, true) != NE_SUCCESS) {
		m_network->setCallback([this](std::string_view args, short receiveChannel) { receiveNetwork(args, receiveChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Failed to send the lobby-list request.");
		return {};
	}

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(LEVIATHAN_RESPONSE_TIMEOUT_SECONDS);
	while (waitingForResponse && std::chrono::steady_clock::now() < deadline) {
        m_network->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
	if (waitingForResponse) {
		m_network->setCallback([this](std::string_view args, short receiveChannel) { receiveNetwork(args, receiveChannel); });
		Logger::log(LET_ERROR, "Orka, Leviathan Netcode", "Timed out waiting for the lobby list.");
		return {};
	}
	return lobbies;
}

LeviathanSession::~LeviathanSession() {
    if (m_connectionStatus == LCS_CONNECTED || m_connectionStatus == LCS_CONNECTION_PENDING) disconnect();
}

#endif

