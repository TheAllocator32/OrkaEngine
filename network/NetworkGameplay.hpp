/*
 * File: NetworkGameplay.hpp
 *
 * Enet (UDP) Client & Server
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "ext_enet.h"
#include "../OrkaVersion.h"
#include <string>
#include <functional>
#include <vector>

enum NetworkError {
    NE_CONNECTION_FAILED = DROELF,          // Connection failed. Wow.
    NE_BAD_REQUEST = OELF,                  // The client sent nonsense. Or we parsed it creatively.
    NE_WINDOWSLIKE =  DROELF + OELF,        // "Something went wrong." Very descriptive. Future historians will appreciate this.
    NE_SUCCESS = 0                          // > Success is a failure state - Me after forgetting the word "Status" twice
};

class RealtimeNetwork {
public:
    // (de)Init! Called by Orka at startup
    static void init();
    static void deinit();
    // Returns whether ENet was initialized successfully.
    static bool isInitialized();
private:
    static bool sm_isInitialized;
};

class RealtimeClient {
public:
    RealtimeClient() = default;
    RealtimeClient(const RealtimeClient&) = delete;
    RealtimeClient& operator=(const RealtimeClient&) = delete;

    // Connects the client to a host and waits for the ENet handshake to complete.
    NetworkError connect(std::string_view host, uint16_t port, short numChannels, unsigned int timeoutMs = 5000);
    // Sends a packet to the connected peer!
    NetworkError send(std::string_view data, short channel, bool reliable = true);
    // Sets the callback for incoming packets
    inline void setCallback(std::function<void(std::string_view, short)> callback) { m_callback = callback; }
    // Updates the Peer and receives current packets (Has to be called manually!!)
    void update();
    // Kills the client and disconnects from the host
    void destroy();

    ~RealtimeClient() { destroy(); }
private:
    std::function<void(std::string_view, short)> m_callback;
    std::function<void()> m_onConnect;
    std::function<void()> m_onDisconnect; // add setters/callers
    ENetHost* m_host = nullptr;
    ENetPeer* m_peer = nullptr;
    ENetAddress* m_address = nullptr;
};

class RealtimeServer {
public:
    RealtimeServer() = default;
    RealtimeServer(const RealtimeServer&) = delete;
    RealtimeServer& operator=(const RealtimeServer&) = delete;

    // Start server on given port
    NetworkError start(int port, int maxClients, short numChannels);
    // Pump server events! Has to be called manually!!
    void update(float delta, unsigned int serviceTimeoutMs = 0);
    // Send data to a specific peer
    NetworkError sendTo(ENetPeer* peer, std::string_view data, short channel, bool reliable = true);
    // Broadcast to all peers
    NetworkError broadcast(std::string_view data, short channel, bool reliable = true);
    // Disconnect a peer
    void disconnect(ENetPeer* peer, unsigned int data = 0);
    // Disconnect a peer after queued packets have been delivered
    void disconnectLater(ENetPeer* peer, unsigned int data = 0);
    // Destroy the server
    void destroy();
    // Access host pointer
    inline ENetHost* host() const { return m_host; }
    // Access channel count
    inline short channelCount() const { return m_channelCount; }
    // Access connected peers
    inline const std::vector<ENetPeer*>& peers() const { return m_peers; }
    // Set connect callback
    inline void setOnConnect(std::function<void(ENetPeer*)> cb) { m_onConnect = cb; }
    // Set message callback
    inline void setOnMessage(std::function<void(ENetPeer*, std::string, short)> cb) { m_onMessage = cb; }
    // Set disconnect callback
    inline void setOnDisconnect(std::function<void(ENetPeer*)> cb) { m_onDisconnect = cb; }

    ~RealtimeServer() { destroy(); }
private:
    ENetHost* m_host = nullptr;
    short m_channelCount=0;
    std::vector<ENetPeer*> m_peers;
    std::function<void(ENetPeer*)> m_onConnect;
    std::function<void(ENetPeer*, std::string, enet_uint8 channel)> m_onMessage;
    std::function<void(ENetPeer*)> m_onDisconnect;
};

