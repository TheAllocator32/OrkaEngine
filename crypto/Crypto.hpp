/*
 * File: Crypto.hpp
 *
 * Orka Crypto module - encrypts files and strings, generates hashes and keys...
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#undef CP_NONE // Thanks Windows :/

enum CryptoProfile { CP_NONE = 0, CP_LIGHT = 1, CP_FULL = 2 };
enum CryptoRole { CR_CLIENT = 0, CR_SERVER = 1 };

class CryptoSession {
public:
    // Constructs a session with id, role and profile
    CryptoSession(std::string_view sessionId, CryptoRole role, CryptoProfile profile = CP_FULL);

    // Sets a pre-shared key (PSK) for this session
    bool setPsk(std::span<const uint8_t> psk);

    // Client begins ECDH handshake (returns hello blob)
    std::vector<uint8_t> beginClientHello();
    // Client finishes handshake using server hello
    bool finishClientHandshake(std::span<const uint8_t> serverHello);
    // Server handles client hello (returns server hello)
    std::vector<uint8_t> handleClientHello(std::span<const uint8_t> clientHello);
    // Server finalizes handshake and derives keys
    bool finalizeServer();

    // Encrypts string using session keys and returns raw binary ciphertext.
    std::string encryptString(std::string_view plaintext);
    // Decrypts raw binary ciphertext using session keys.
    std::string decryptString(std::string_view ciphertext);

    // Hashes string with BLAKE2b-256 (hex)
    static std::string hashString(std::string_view data);

    // Computes keyed MAC (hex) in Light/Full profiles
    std::string macString(std::string_view data);
    // Verifies keyed MAC (hex)
    bool verifyMac(std::string_view data, std::string_view macHex);

    // Returns readiness
    inline bool isReady() const { return m_ready; }

    // Accessors
    CryptoProfile profile() const { return m_profile; }
    std::string sessionId() const { return m_sessionId; }

private:
    void randomBytes(void* dst, size_t n) const;
    void deriveSessionKeysFromShared(const std::array<uint8_t, 32>& shared, std::span<const uint8_t> ctx);
    void deriveSessionKeysFromPsk(std::span<const uint8_t> psk);
    void makeNonce(std::array<uint8_t, 24>& outNonce, bool c2s, uint64_t seq) const;

    static std::string toHex(std::span<const uint8_t> bin);
    static bool fromHex(std::string_view hex, std::vector<uint8_t>& out);

private:
    std::string m_sessionId;
    CryptoRole m_role;
    CryptoProfile m_profile;
    std::array<uint8_t, 32> m_kC2S;
    std::array<uint8_t, 32> m_kS2C;
    std::array<uint8_t, 32> m_kMac;
    std::array<uint8_t, 16> m_connId;
    uint64_t m_seqEnc = 0;
    uint64_t m_seqDec = 0;
    std::array<uint8_t, 32> m_xSk;
    std::array<uint8_t, 32> m_xPk;
    std::array<uint8_t, 32> m_peerPk;
    bool m_handshakeDone = false;
    bool m_ready = false;
    static inline void (*sm_randomCb)(void*, size_t) = nullptr;
};

