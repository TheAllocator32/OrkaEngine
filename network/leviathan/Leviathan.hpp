/*
 * File: Leviathan.hpp
 *
 * Leviathan is Orka's advanced reference implementation of the DSACIRC-AEV(tm) Networking Protocol
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../NetworkGameplay.hpp"
#include "../../crypto/Crypto.hpp"
#include "LeviathanTypes.hpp"
#include "../../OrkaVersion.h"
#include <unordered_map>
#include <string_view>
#include <functional>
#include <string>
#include <vector>
#include <span>
#include <utility>

// Config
#define LEVIATHAN_RESPONSE_TIMEOUT_SECONDS 5

/*
 * Leviathan supports 5 different communication mechanisms
 *  1. Negotiation/Vote
 *      - Used to vote for a spec
 *  2. Unreliable "input" broadcast
 *      - Used for frequently repeated character simulation data (movement, rotation...)
 *  3. Reliable event "input" broadcast
 *      - Used for rare character simulation events (e.g. shoot)
 *  4. State synchronization broadcast (host only)
 *      - Host sends scene snapshot all clients update
 *  5. Host event broadcast (host only)
 *      - Host sends data. Separate channel because only this way server can make sure no one pretends to be host
 */

class LeviathanSession {
public:
    // Constructor and Destructor
    LeviathanSession() = default;
    LeviathanSession(const LeviathanSession&) = delete;
    LeviathanSession& operator=(const LeviathanSession&) = delete;
    ~LeviathanSession();

    // Establishes connection to a Leviathan server and encrypts the connection
    void connect(std::string_view address, uint16_t port=12522);
    // Disconnect from Leviathan server (forces disconnection, call leave() before to leave the current lobby) Resets/clears session (so you can reuse it cleanly)
    void disconnect();

    // Connects this session to the selected lobby on the currently connected server. Returns true on success
    bool connectToLobby(int lobbyID);

    // Returns connection status
    inline LeviathanConnectionStatus getConnectionStatus() const { return m_connectionStatus; }

    // Sets login data (transmitted to all players in lobby). Verification token is optional and used to prove you are actually *username* It must be a lambda returning std::string and should generate unique verification tokens. Username must be unique in the session!
    inline void setCredentials(std::string_view username, const std::function<std::string()>& verificationToken=0) { m_username = username; m_verificationTokenGenerator = verificationToken; }

    // Returns all players currently connected to your lobby. Note that the Participant::verificationToken usually invalidates after "player-is-valid" check (Depending on your account server implementation).
    inline std::span<const LeviathanParticipant> getParticipants() const { return m_participants; }

    // Returns the Leviathan version
    inline static constexpr std::string_view getVersion() { return LEVIATHAN_VERSION_STRING; }

    // Returns true if this Session is Leviathan Host!
    bool isHost() const { return m_sessionIsHost; }
    // Returns true if a game/match is running
    bool isGameRunning() const { return m_gameRunning; }
    // Returns the connected lobby ID, or -1 when this session is not in a lobby.
    int getConnectedLobby() const { return m_connectedLobby; }
    // Returns the remaining lobby countdown in seconds, or -1 when no countdown is running.
    int getLobbyCountdownSeconds() const { return m_lobbyCountdownSeconds; }
    // Returns this session's server-assigned, lobby-scoped participant ID, or -1 outside a lobby.
    int getParticipantID() const { return m_participantID; }

    // Manually sends data on the selected channel (Don't use unless you know what you are doing!! And if you aren't me you probably don't...)
    void send(std::string_view payload, LeviathanDataChannel channel = LDC_GAMEPLAY);

    // Leaves the current game/lobby gracefully (and disconnects from the server)
    void leave();

    // Votes to start the current lobby's game when the server permits manual starts.
    void voteStartGame() const;

    // Signals the end of the current match and returns all players to the lobby. Call when game is finished (win/loose conditions reached) (host only) (do not expose to GUI!!!)
    void endGame();

    // Returns all available Lobbies and their status. Only works after successful connection (after encryption/LCS_CONNECTED). Returns nothing on failure. Avoid running update() in a separate thread in the meanwhile.
    std::vector<LeviathanLobby> fetchLobbies();

    // Votes on a specific subject for a specific value
    void vote(std::string_view subject, std::string_view value);
    void multiVote(std::string_view subject, std::span<std::string_view> values);
    // Returns current vote results on a specific subject
    inline std::span<const VoteResult> getVoteResults(std::string_view subject) const { auto it = m_voteResults.find(std::string(subject)); return it != m_voteResults.end() ? std::span<const VoteResult>(it->second) : std::span<const VoteResult>(); }
    inline std::span<const MultiVoteResult> getMultiVoteResults(std::string_view subject) const { auto it = m_multiVoteResults.find(std::string(subject)); return it != m_multiVoteResults.end() ? std::span<const MultiVoteResult>(it->second) : std::span<const MultiVoteResult>(); }

    // Sends input (unreliable). Use for frequently sent data like movement and rotation
    void sendInput(std::string_view action, std::string_view value, LeviathanActionType type);
    void sendInput(uint16_t action, uint32_t value, LeviathanActionType type);
    void sendInput(uint16_t action, float value, LeviathanActionType type);
    // Sends input (reliable). Use for rare, important things like "shoot" in Shooter Games
    void sendInputReliable(std::string_view action, std::string_view value, LeviathanActionType type);
    // Sets callback called when receiving inputs if type LAT_CUSTOM
    inline void setCustomInputActionCallback(const std::function<void(std::string_view action, std::string_view value)> &callback) { m_customInputActionCallback = callback; }

    // Sends scene correction data. Pass SceneNode IDs to exclude (HOST ONLY!)
    void sendSceneHostCorrection(std::span<Identifier> exclude);

    // Sends host event (HOST ONLY!)
    void sendHostEvent(std::string_view event, std::string_view value);
    // Sets callback called when receiving inputs if type LAT_CUSTOM
    inline void setHostEventCallback(const std::function<void(std::string_view action, std::string_view value)> &callback) { m_hostEventCallback = callback; }

    // CALL THIS EVERY FRAME!!! Preferably in GameMode::update(). IF YOU FORGET IT NOTHING WILL HAPPEN AND YOU WILL BE SAD :(
    void update();
private:
    // Receive functions called by RealtimeClient!
    void receiveNetwork(std::string_view data, short channel);
    void receiveNetworkEncrypt(std::string_view data, short channel);
    void reset();

    // Internal helpers:
    // Sends username and token to all players
    bool sendHello(bool iamnew);
private:
    RealtimeClient* m_network = nullptr; // Internal connection
    CryptoSession* m_crypto = nullptr;
    std::string m_username;
    std::function<std::string()> m_verificationTokenGenerator;
    bool m_gameRunning = false; // Lobby mode if false
    bool m_sessionIsHost = false; // The host flag. If you think setting this makes you the host, the server disagrees and the only effect will be higher CPU load ;)
    int m_lobbyCountdownSeconds = -1;
    int m_connectedLobby = -1;
    int m_participantID = -1;
    std::string m_serverAddress;
    LeviathanConnectionStatus m_connectionStatus = LCS_DISCONNECTED;
    std::vector<LeviathanParticipant> m_participants;
    std::unordered_map<std::string, std::vector<VoteResult>> m_voteResults; // subject, array of vote results
    std::unordered_map<std::string, std::vector<MultiVoteResult>> m_multiVoteResults; // subject, array of vote results
    std::function<void(std::string_view action, std::string_view value)> m_customInputActionCallback;
    std::function<void(std::string_view action, std::string_view value)> m_hostEventCallback;
};

