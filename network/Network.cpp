#ifndef ORKA_DISABLE_NETWORKING
/*
 * File: Network.cpp
 *
 * The implementation of the HTTP and HTTPS component
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Network.hpp"
#include "ext_httplib.h"
#include <fstream>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>

static std::pair<std::string, std::string> split_host_path(const std::string& url) {
    std::string trimmed = url.substr(url.find("://") + 3);
    auto slash = trimmed.find('/');
    if (slash == std::string::npos)
        return { trimmed, "/" };
    return { trimmed.substr(0, slash), trimmed.substr(slash) };
}

static std::unique_ptr<httplib::Client> create_client(const std::string& url) {
    auto [host, _] = split_host_path(url);
    return std::make_unique<httplib::Client>(url.substr(0, url.find('/', 8)).c_str()); // e.g. "https://example.com:443"
}

void Network::GET(std::string_view url, std::function<void(std::string, int)> finish) {
    const std::string urlString(url);
    std::thread([urlString, finish]() {
        auto [_, path] = split_host_path(urlString);
        auto cli = create_client(urlString);
        if (auto res = cli->Get(path.c_str()))
            finish(res->body, res->status);
        else
            finish("", 0);
    }).detach();
}

void Network::POST(std::string_view url, std::string_view body, std::string_view contentType, std::function<void(std::string, int)> finish) {
    const std::string urlString(url);
    const std::string bodyString(body);
    const std::string contentTypeString(contentType);
    std::thread([urlString, bodyString, contentTypeString, finish]() {
        auto [_, path] = split_host_path(urlString);
        auto cli = create_client(urlString);
        if (auto res = cli->Post(path.c_str(), bodyString, contentTypeString.c_str()))
            finish(res->body, res->status);
        else
            finish("", 0);
    }).detach();
}

void Network::downloadFile(std::string_view url, std::string_view file_path, std::function<void(float)> progress, std::function<void(bool)> finish) {
    const std::string urlString(url);
    const std::string filePathString(file_path);
    std::thread([urlString, filePathString, progress, finish]() {
        auto [_, path] = split_host_path(urlString);
        auto cli = create_client(urlString);

        std::ofstream ofs(filePathString, std::ios::binary);
        if (!ofs) {
            if (finish) finish(false);
            return;
        }

        size_t total = 0;
        size_t expected_size = 0;
        bool has_length = false;

        auto res = cli->Get(path.c_str(),
            [&](const httplib::Response& r) {
                auto it = r.headers.find("Content-Length");
                if (it != r.headers.end()) {
                    expected_size = std::stoull(it->second);
                    has_length = true;
                } else {
                    has_length = false;
                    if (progress) progress(-1.0f);
                }
                return true;
            },
            [&](const char* data, size_t len) {
                ofs.write(data, len);
                total += len;

                if (has_length && progress) {
                    float percent = (float)total / expected_size * 100.0f;
                    progress(percent);
                }
                return true;
            });

        ofs.close();

        if (!res || res->status != 200) {
            if (finish) finish(false);
            return;
        }

        if (finish) finish(true);
     }).detach();
}

std::string Network::GETSync(std::string_view url) {
    const std::string urlString(url);
    auto [_, path] = split_host_path(urlString);
    auto cli = create_client(urlString);
    if (auto res = cli->Get(path.c_str()))
        return res->body;
    return {};
}

std::string Network::POSTSync(std::string_view url, std::string_view body, std::string_view contentType) {
    const std::string urlString(url);
    const std::string bodyString(body);
    const std::string contentTypeString(contentType);
    auto [_, path] = split_host_path(urlString);
    auto cli = create_client(urlString);
    if (auto res = cli->Post(path.c_str(), bodyString, contentTypeString.c_str()))
        return res->body;
    return {};
}

void Network::downloadFileSync(std::string_view url, std::string_view file_path) {
    const std::string urlString(url);
    const std::string filePathString(file_path);
    auto [_, path] = split_host_path(urlString);
    auto cli = create_client(urlString);

    std::ofstream ofs(filePathString, std::ios::binary);
    if (!ofs) return;

    cli->Get(path.c_str(), [&](const char* data, size_t len) {
        ofs.write(data, len);
        return true;
    });

    ofs.close();
}

#endif

