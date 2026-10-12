#ifndef ORKA_DISABLE_CRYPTO
/*
 * File: Crypto.hpp
 *
 * Orka Crypto module - encrypts files and strings, generates hashes and keys... - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Crypto.hpp"
#include "ext_monocypher.h"
#include "../essentials/Logger.hpp"
#include <cstring>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <vector>
#include <span>
#include <random>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#elif defined(__linux__)
#include <sys/random.h>
#include <errno.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <errno.h>
#endif

#if defined(_WIN32)
extern "C" __declspec(dllimport)
long __stdcall BCryptGenRandom(void* hAlg,
    unsigned char* pbBuffer, unsigned long cbBuffer, unsigned long dwFlags);
#pragma comment(lib, "bcrypt.lib")
#endif

static bool osRandom(void* dst, size_t n) {
    if (n == 0) return true;
#if defined(_WIN32)
    return BCryptGenRandom(nullptr,
        static_cast<unsigned char*>(dst),
        (unsigned long)n,
        0x00000002UL) == 0; // BCRYPT_USE_SYSTEM_PREFERRED_RNG

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    arc4random_buf(dst, n);
    return true;
#elif defined(__linux__)
    size_t off = 0;
    while (off < n) {
        ssize_t r = getrandom((char*)dst + off, n - off, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        off += (size_t)r;
    }
    if (off == n) return true;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        size_t off = 0;
        while (off < n) {
            ssize_t r = read(fd, (char*)dst + off, n - off);
            if (r < 0) { if (errno == EINTR) continue; break; }
            if (r == 0) break;
            off += (size_t)r;
        }
        close(fd);
        if (off == n) return true;
    }
#endif

    std::random_device rd;
    if (rd.entropy() > 0.0) {
        uint8_t* p = static_cast<uint8_t*>(dst);
        size_t remain = n;
        while (remain) {
            uint32_t x = ((uint32_t)rd() << 16) ^ (uint32_t)rd();
            size_t take = remain < sizeof(x) ? remain : sizeof(x);
            std::memcpy(p, &x, take);
            p += take;
            remain -= take;
        }
        return true;
    }

    Logger::log(LET_WARN, "Orka, Crypto", "AAAAh! No secure RNG available -> refusing to generate random bytes!");
    return false;
}


CryptoSession::CryptoSession(std::string_view sessionId, CryptoRole role, CryptoProfile profile)
    : m_sessionId(sessionId), m_role(role), m_profile(profile) {
    randomBytes(m_connId.data(), m_connId.size());
    randomBytes(m_xSk.data(), m_xSk.size());
    crypto_x25519_public_key(m_xPk.data(), m_xSk.data());
    if (m_profile == CP_NONE) m_ready = true;
}

void CryptoSession::randomBytes(void* dst, size_t n) const {
    if (sm_randomCb) { sm_randomCb(dst, n); return; }
    if (!osRandom(dst, n)) throw std::runtime_error("RNG failed");
}

bool CryptoSession::setPsk(std::span<const uint8_t> psk) {
    if (m_profile == CP_NONE) { m_ready = true; return true; }
    deriveSessionKeysFromPsk(psk);
    m_ready = true;
    m_handshakeDone = true;
    return true;
}

void CryptoSession::deriveSessionKeysFromPsk(std::span<const uint8_t> psk) {
    auto kdf = [&](std::array<uint8_t, 32>& out, const char* tag) {
        crypto_blake2b_keyed(out.data(), 32, psk.data(), psk.size(), (const uint8_t*)tag, std::strlen(tag));
        };
    kdf(m_kMac, "ORKA:MAC");
    kdf(m_kC2S, "ORKA:C2S");
    kdf(m_kS2C, "ORKA:S2C");
}

std::vector<uint8_t> CryptoSession::beginClientHello() {
    std::vector<uint8_t> out(48);
    std::memcpy(out.data() + 0, m_xPk.data(), 32);
    randomBytes(out.data() + 32, 16);
    return out;
}

bool CryptoSession::finishClientHandshake(std::span<const uint8_t> serverHello) {
    if (serverHello.size() < 48) return false;
    std::memcpy(m_peerPk.data(), serverHello.data() + 0, 32);
    std::array<uint8_t, 32> shared;
    crypto_x25519(shared.data(), m_xSk.data(), m_peerPk.data());
    std::vector<uint8_t> ctx(m_sessionId.begin(), m_sessionId.end());
    deriveSessionKeysFromShared(shared, ctx);
    crypto_wipe(shared.data(), shared.size());
    m_handshakeDone = true;
    m_ready = true;
    return true;
}

std::vector<uint8_t> CryptoSession::handleClientHello(std::span<const uint8_t> clientHello) {
    if (clientHello.size() < 48) return {};
    std::memcpy(m_peerPk.data(), clientHello.data() + 0, 32);
    std::vector<uint8_t> out(48);
    std::memcpy(out.data() + 0, m_xPk.data(), 32);
    randomBytes(out.data() + 32, 16);
    return out;
}

bool CryptoSession::finalizeServer() {
    std::array<uint8_t, 32> shared;
    crypto_x25519(shared.data(), m_xSk.data(), m_peerPk.data());
    std::vector<uint8_t> ctx(m_sessionId.begin(), m_sessionId.end());
    deriveSessionKeysFromShared(shared, ctx);
    crypto_wipe(shared.data(), shared.size());
    m_handshakeDone = true;
    m_ready = true;
    return true;
}

void CryptoSession::deriveSessionKeysFromShared(const std::array<uint8_t, 32>& shared, std::span<const uint8_t> ctx) {
    std::vector<uint8_t> buf;
    buf.reserve(32 + 4 + ctx.size());
    buf.insert(buf.end(), shared.begin(), shared.end());
    buf.insert(buf.end(), { 'O','R','K','A' });
    buf.insert(buf.end(), ctx.begin(), ctx.end());
    std::array<uint8_t, 32> master;
    crypto_blake2b(master.data(), 32, buf.data(), buf.size());
    auto prf = [&](std::array<uint8_t, 32>& out, const char* dir) {
        crypto_blake2b_keyed(out.data(), 32, master.data(), master.size(), (const uint8_t*)dir, std::strlen(dir));
        };
    prf(m_kMac, "MAC");
    prf(m_kC2S, "C2S");
    prf(m_kS2C, "S2C");
    crypto_wipe(master.data(), master.size());
}

void CryptoSession::makeNonce(std::array<uint8_t, 24>& outNonce, bool c2s, uint64_t seq) const {
    std::memset(outNonce.data(), 0, outNonce.size());
    std::memcpy(outNonce.data() + 0, m_connId.data(), 16);
    uint64_t v = seq;
    if (c2s) v |= (1ull << 63); else v &= ~(1ull << 63);
    std::memcpy(outNonce.data() + 16, &v, 8);
}

std::string CryptoSession::encryptString(std::string_view plaintext) {
    if (!m_ready) throw std::runtime_error("CryptoSession not ready! What the heck have you done?");
    if (m_profile == CP_NONE) {
        return std::string(plaintext);
    }
    if (m_profile == CP_LIGHT) {
        constexpr std::size_t macSize = 32;
        if (plaintext.size() > std::numeric_limits<std::size_t>::max() - macSize)
            throw std::length_error("Plaintext too large");
        std::array<uint8_t, macSize> macRaw;
        crypto_blake2b_keyed(macRaw.data(), macRaw.size(), m_kMac.data(), m_kMac.size(), reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size());
        std::vector<uint8_t> wire(macSize + plaintext.size());
        std::memcpy(wire.data(), macRaw.data(), macSize);
        std::memcpy(wire.data() + macSize, plaintext.data(), plaintext.size());
        return std::string(reinterpret_cast<const char*>(wire.data()), wire.size());
    }
    const std::array<uint8_t, 32>& key = (m_role == CR_CLIENT) ? m_kC2S : m_kS2C;
    bool dirC2S = (m_role == CR_CLIENT);
    std::array<uint8_t, 24> nonce;
    makeNonce(nonce, dirC2S, ++m_seqEnc);
    std::vector<uint8_t> ct(plaintext.size());
    std::array<uint8_t, 16> mac;
    crypto_aead_lock(ct.data(), mac.data(), key.data(), nonce.data(), nullptr, 0, (const uint8_t*)plaintext.data(), plaintext.size());
    std::vector<uint8_t> wire(24 + 16 + ct.size());
    std::memcpy(wire.data() + 0, nonce.data(), 24);
    std::memcpy(wire.data() + 24, mac.data(), 16);
    std::memcpy(wire.data() + 40, ct.data(), ct.size());
    return std::string(reinterpret_cast<const char*>(wire.data()), wire.size());
}

std::string CryptoSession::decryptString(std::string_view ciphertext) {
    if (!m_ready) throw std::runtime_error("CryptoSession not ready man");
    if (m_profile == CP_NONE) {
        return std::string(ciphertext);
    }
    if (m_profile == CP_LIGHT) {
        if (ciphertext.size() < 32) throw std::runtime_error("MAC missing");
        std::string plain(ciphertext.substr(32));
        std::array<uint8_t, 32> calc;
        crypto_blake2b_keyed(calc.data(), 32, m_kMac.data(), m_kMac.size(), (const uint8_t*)plain.data(), plain.size());
        if (crypto_verify32(calc.data(), reinterpret_cast<const uint8_t*>(ciphertext.data())) != 0) throw std::runtime_error("MAC fail");
        return plain;
    }
    if (ciphertext.size() < 40) throw std::runtime_error("Cipher too short");
    const std::array<uint8_t, 32>& keyIn = (m_role == CR_CLIENT) ? m_kS2C : m_kC2S;
    std::array<uint8_t, 24> nonce;
    std::memcpy(nonce.data(), ciphertext.data() + 0, 24);
    std::array<uint8_t, 16> mac;
    std::memcpy(mac.data(), ciphertext.data() + 24, 16);
    std::span<const uint8_t> ct{ reinterpret_cast<const uint8_t*>(ciphertext.data() + 40), ciphertext.size() - 40 };
    std::vector<uint8_t> plain(ct.size());
    int ok = crypto_aead_unlock(plain.data(), mac.data(), keyIn.data(), nonce.data(), nullptr, 0, ct.data(), ct.size());
    if (ok != 0) throw std::runtime_error("Decrypt/Auth failed");
    return std::string((const char*)plain.data(), plain.size());
}

std::string CryptoSession::hashString(std::string_view data) {
    std::array<uint8_t, 32> h;
    crypto_blake2b(h.data(), 32, (const uint8_t*)data.data(), data.size());
    static const char* d = "0123456789abcdef";
    std::string s; s.resize(h.size() * 2);
    for (size_t i = 0; i < h.size(); ++i) { s[2 * i] = d[(h[i] >> 4) & 0xF]; s[2 * i + 1] = d[h[i] & 0xF]; }
    return s;
}

std::string CryptoSession::macString(std::string_view data) {
    std::array<uint8_t, 32> mac;
    crypto_blake2b_keyed(mac.data(), 32, m_kMac.data(), m_kMac.size(), (const uint8_t*)data.data(), data.size());
    static const char* d = "0123456789abcdef";
    std::string s; s.resize(mac.size() * 2);
    for (size_t i = 0; i < mac.size(); ++i) { s[2 * i] = d[(mac[i] >> 4) & 0xF]; s[2 * i + 1] = d[mac[i] & 0xF]; }
    return s;
}

bool CryptoSession::verifyMac(std::string_view data, std::string_view macHex) {
    if (macHex.size() != 64) return false;
    std::array<uint8_t, 32> mac;
    for (size_t i = 0; i < 32; ++i) {
        auto hv = [](char c)->int { if (c >= '0' && c <= '9')return c - '0'; if (c >= 'a' && c <= 'f')return c - 'a' + 10; if (c >= 'A' && c <= 'F')return c - 'A' + 10; return -1; };
        int hi = hv(macHex[2 * i]); int lo = hv(macHex[2 * i + 1]); if (hi < 0 || lo < 0) return false; mac[i] = uint8_t((hi << 4) | lo);
    }
    std::array<uint8_t, 32> calc;
    crypto_blake2b_keyed(calc.data(), 32, m_kMac.data(), m_kMac.size(), (const uint8_t*)data.data(), data.size());
    return crypto_verify32(calc.data(), mac.data()) == 0;
}

std::string CryptoSession::toHex(std::span<const uint8_t> bin) {
    static const char* d = "0123456789abcdef";
    std::string s; s.resize(bin.size() * 2);
    for (size_t i = 0; i < bin.size(); ++i) { s[2 * i] = d[(bin[i] >> 4) & 0xF]; s[2 * i + 1] = d[bin[i] & 0xF]; }
    return s;
}

bool CryptoSession::fromHex(std::string_view hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2) return false;
    out.resize(hex.size() / 2);
    auto hv = [](char c)->int { if (c >= '0' && c <= '9')return c - '0'; if (c >= 'a' && c <= 'f')return c - 'a' + 10; if (c >= 'A' && c <= 'F')return c - 'A' + 10; return -1; };
    for (size_t i = 0; i < out.size(); ++i) { int hi = hv(hex[2 * i]); int lo = hv(hex[2 * i + 1]); if (hi < 0 || lo < 0) return false; out[i] = uint8_t((hi << 4) | lo); }
    return true;
}

#endif
