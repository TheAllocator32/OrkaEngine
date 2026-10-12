#ifndef ORKA_DISABLE_NETWORKING
/*
 * File: NetworkGameplay.cpp
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

#include "../essentials/Logger.hpp"
#include "NetworkGameplay.hpp"
#include <chrono>
#include <cstdlib>

// Static variable definition
bool RealtimeNetwork::sm_isInitialized = false;

void RealtimeNetwork::init() {
    if (sm_isInitialized) return;

    if (enet_initialize() != 0) {
        Logger::log(LET_CRITICAL_ERROR, "Orka, Networking", "Could not initialize UDP system!! Multiplayer Gameplay will NOT BE POSSIBLE!!!");
        return;
    }
    sm_isInitialized = true;
}

void RealtimeNetwork::deinit() {
    if (sm_isInitialized) {
        enet_deinitialize();
    }
    sm_isInitialized = false;
}

bool RealtimeNetwork::isInitialized() {
    return sm_isInitialized;
}

static void requireRealtimeNetworkInitialization(std::string_view operation) {
    if (RealtimeNetwork::isInitialized()) return;

    Logger::log(LET_CRITICAL_ERROR, "Orka, Networking",
        "Realtime " + std::string(operation) + " requested before RealtimeNetwork::init() was called.");
    std::exit(EXIT_FAILURE);
}

NetworkError RealtimeClient::connect(std::string_view host, uint16_t port, short numChannels, unsigned int timeoutMs) {
    requireRealtimeNetworkInitialization("client connect");

    destroy();
    m_host = enet_host_create(nullptr, 1, numChannels, 0, 0);
    if (!m_host) return NE_CONNECTION_FAILED;

    if (!m_address) m_address = new ENetAddress{};
    const std::string hostString(host);
    enet_address_set_host(m_address, hostString.c_str());
    m_address->port = static_cast<enet_uint16>(port);

    m_peer = enet_host_connect(m_host, m_address, numChannels, 0);
    if (!m_peer) {
        destroy();
        return NE_CONNECTION_FAILED;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    ENetEvent event{};
    while (std::chrono::steady_clock::now() < deadline) {
        const int serviceResult = enet_host_service(m_host, &event, 10);
        if (serviceResult < 0) {
            Logger::log(LET_ERROR, "Orka, Networking", "Failed while waiting for the server connection handshake.");
            destroy();
            return NE_CONNECTION_FAILED;
        }
        if (serviceResult == 0) continue;

        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            m_peer = event.peer;
            if (m_onConnect) m_onConnect();
            return NE_SUCCESS;
        }
        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            Logger::log(LET_ERROR, "Orka, Networking", "Server disconnected before the connection handshake completed.");
            m_peer = nullptr;
            destroy();
            return NE_CONNECTION_FAILED;
        }
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(event.packet);
        }
    }

    Logger::log(LET_ERROR, "Orka, Networking", "Timed out waiting for the server connection handshake.");
    destroy();
    return NE_CONNECTION_FAILED;
}

NetworkError RealtimeClient::send(std::string_view data, short channel, bool reliable) {
    requireRealtimeNetworkInitialization("client send");

    if (!m_host || !m_peer) {
        Logger::log(LET_ERROR, "Orka, Networking", "Send failed: Invalid state (host or peer null)! Init and connect first!");
        return NE_BAD_REQUEST;
    }
    int flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;

    ENetPacket* packet = enet_packet_create(data.data(), data.size(), flags);
    if (!packet) {
        Logger::log(LET_ERROR, "Orka, Networking", "Packet creation failed (Hopefully YOU know why)");
        return NE_WINDOWSLIKE;
    }

    if (enet_peer_send(m_peer, static_cast<enet_uint8>(channel), packet) < 0) {
        Logger::log(LET_ERROR, "Orka, Networking", "Send failed (possibly channel full or disconnected)");
        enet_packet_destroy(packet);
        return NE_CONNECTION_FAILED;
    }
    enet_host_flush(m_host);

    return NE_SUCCESS;
}

void RealtimeClient::update() {
    requireRealtimeNetworkInitialization("client update");

    if (!m_host) return;

    ENetEvent event;
    while (enet_host_service(m_host, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            if (m_onConnect) m_onConnect();
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            std::string msg(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
            // Keep the active callback alive while it runs, even if it replaces itself.
            auto callback = m_callback;
            if (callback) callback(msg, static_cast<short>(event.channelID));
            enet_packet_destroy(event.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            Logger::log(LET_WARN, "Orka, Networking", "Server disconnected!");
            if (m_onDisconnect) m_onDisconnect();
            m_peer = nullptr;
            break;
        }
        default:
            break;
        }
    }

    enet_host_flush(m_host);
}

void RealtimeClient::destroy() {
    if (!m_host && !m_peer) return;
    requireRealtimeNetworkInitialization("client destroy");

    if (m_peer) {
        enet_peer_disconnect_now(m_peer, 0);
        m_peer = nullptr;
    }
    if (m_host) {
        enet_host_destroy(m_host);
        m_host = nullptr;
    }
}

NetworkError RealtimeServer::start(int port, int maxClients, short numChannels) {
    requireRealtimeNetworkInitialization("server start");

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(port);
    m_host = enet_host_create(&address, static_cast<size_t>(maxClients), static_cast<size_t>(numChannels), 0, 0);
    if (!m_host) {
        Logger::log(LET_CRITICAL_ERROR, "Orka, Networking", "Failed to create server host!");
        return NE_CONNECTION_FAILED;
    }
    m_channelCount = numChannels;
    m_peers.clear();
    Logger::log(LET_INFO, "Orka, Networking", "Server started on port " + std::to_string(port));
    return NE_SUCCESS;
}

static std::string addressToString(const ENetAddress& addr) {
    char buf[INET6_ADDRSTRLEN] = { 0 };
    if (inet_ntop(AF_INET6, &addr.host, buf, sizeof(buf)) == nullptr) {
        return "<invalid>";
    }
    return std::string(buf) + ":" + std::to_string(addr.port);
}

void RealtimeServer::update(float, unsigned int serviceTimeoutMs) {
    requireRealtimeNetworkInitialization("server update");

    if (!m_host) return;
    ENetEvent event;
    while (enet_host_service(m_host, &event, serviceTimeoutMs) > 0) {
        char ip[64];
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            m_peers.push_back(event.peer);
            if (enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip)) == 0) {
                Logger::log(LET_SPECIAL, "Orka, Networking",
                    std::string("Client connected (") + ip + ":" + std::to_string(event.peer->address.port) + ")");
            } else {
                Logger::log(LET_WARN, "Orka, Networking", "Client connected (ip: <unknown>)");
            }
            if (m_onConnect) m_onConnect(event.peer);
            break;
        case ENET_EVENT_TYPE_RECEIVE: {
            if (m_onMessage) {
                std::string msg(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                m_onMessage(event.peer, msg, event.channelID);
            }
            enet_packet_destroy(event.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            for (auto it = m_peers.begin(); it != m_peers.end(); ++it) if (*it == event.peer) { m_peers.erase(it); break; }
            if (enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip)) == 0) {
                Logger::log(LET_SPECIAL, "Orka, Networking", std::string(event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT ? "Dead client removed during GC cycle (" : "Client disconnected (") + ip + ")");
            } else {
                Logger::log(LET_SPECIAL, "Orka, Networking", event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT ? "Dead client removed during GC cycle" : "Client disconnected");
            }
            if (m_onDisconnect) m_onDisconnect(event.peer);
            event.peer->data = nullptr;
            break;
        default: break;
        }
    }
    enet_host_flush(m_host);
}

NetworkError RealtimeServer::sendTo(ENetPeer* peer, std::string_view data, short channel, bool reliable) {
    requireRealtimeNetworkInitialization("server send");

    if (!m_host || !peer) return NE_BAD_REQUEST;
    const int flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), flags);
    if (!packet) {
        Logger::log(LET_ERROR, "Orka, Networking", "Packet creation failed in sendTo()");
        return NE_WINDOWSLIKE;
    }
    if (enet_peer_send(peer, static_cast<enet_uint8>(channel), packet) < 0) {
        char ip[64] = "unknown";
        const bool hasAddress = enet_address_get_host_ip(&peer->address, ip, sizeof(ip)) == 0;
        Logger::log(LET_ERROR, "Orka, Networking", "Failed to send " + std::to_string(data.size()) +
            " bytes on channel " + std::to_string(channel) + " to peer " +
            (hasAddress ? std::string(ip) + ":" + std::to_string(peer->address.port) : "with an unknown address") +
            "; the peer may already be disconnected");
        enet_packet_destroy(packet);
        return NE_CONNECTION_FAILED;
    }
    enet_host_flush(m_host);
    return NE_SUCCESS;
}

NetworkError RealtimeServer::broadcast(std::string_view data, short channel, bool reliable) {
    requireRealtimeNetworkInitialization("server broadcast");

    if (!m_host) return NE_BAD_REQUEST;
    const int flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), flags);
    if (!packet) {
        Logger::log(LET_ERROR, "Orka, Networking", "Packet creation failed in broadcast()");
        return NE_WINDOWSLIKE;
    }
    enet_host_broadcast(m_host, static_cast<enet_uint8>(channel), packet);
    enet_host_flush(m_host);
    return NE_SUCCESS;
}

void RealtimeServer::disconnect(ENetPeer* peer, unsigned data) {
    requireRealtimeNetworkInitialization("server disconnect");

    if (!peer) return;
    char ip[64] = "unknown";
    const bool hasAddress = enet_address_get_host_ip(&peer->address, ip, sizeof(ip)) == 0;
    Logger::log(LET_INFO, "Orka, Networking", "Disconnecting peer " +
        (hasAddress ? std::string(ip) + ":" + std::to_string(peer->address.port) : "with an unknown address"));
    enet_peer_disconnect(peer, data);
}

void RealtimeServer::disconnectLater(ENetPeer* peer, unsigned data) {
    requireRealtimeNetworkInitialization("server delayed disconnect");

    if (!peer) return;
    char ip[64] = "unknown";
    const bool hasAddress = enet_address_get_host_ip(&peer->address, ip, sizeof(ip)) == 0;
    Logger::log(LET_INFO, "Orka, Networking", "Disconnecting peer after queued packets are delivered: " +
        (hasAddress ? std::string(ip) + ":" + std::to_string(peer->address.port) : "unknown address"));
    enet_peer_disconnect_later(peer, data);
}

void RealtimeServer::destroy() {
    if (!m_host) return;
    requireRealtimeNetworkInitialization("server destroy");

    Logger::log(LET_INFO, "Orka, Networking", "Shutting down server");
    enet_host_flush(m_host);
    enet_host_destroy(m_host);
    m_host = nullptr;
    m_channelCount = 0;
    m_peers.clear();
}

#endif

