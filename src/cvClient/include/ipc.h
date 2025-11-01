#pragma once

#ifndef IPC_H
#define IPC_H

#include <unordered_map>
#include <vector>
#include <functional>
#include <string>

#include <cstdlib>

#include <thread>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ipc {
    class Pipe {
    public:
        Pipe(const char* pipeName);
        ~Pipe();

        int on(std::string event, std::function<void(const char*, size_t)> handler);
        int removeListener(std::string event, int id);
        int removeAllListeners(std::string event);

        int read(const char* buffer, size_t size);
        int write(const char* data, size_t size);

    private:
        std::thread* connectThread;
        std::thread* readThread;
        std::thread* writeThread;

        void emit(std::string& event, const char* buffer, size_t size);

        std::mutex pipeMutex;
        std::unordered_map<std::string, std::vector<std::function<void(const char*, size_t)>>> eventHandlers;
#ifdef _WIN32
        HANDLE hPipe = INVALID_HANDLE_VALUE;
        LPTSTR lpszPipename = nullptr;
#endif
    };
    #endif
}