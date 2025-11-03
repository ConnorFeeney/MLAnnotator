#pragma once

#ifndef IPC_H
#define IPC_H

#include <unordered_map>
#include <vector>
#include <queue>
#include <functional>
#include <string>

#include <cstdlib>
#include <cstddef>

#include <thread>
#include <mutex>
#include <memory>

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

        int read(std::vector<char>);
        int write(std::vector<char>);

    private:
        std::unique_ptr<std::thread> connectThread;
        std::unique_ptr<std::thread> readThread;
        std::unique_ptr<std::thread> writeThread;

        std::mutex eventMutex;
        std::unordered_map<std::string, std::vector<std::function<void(const char*, size_t)>>> eventHandlers;

        std::mutex pipeMutex;
        std::atomic<bool> pipeRunning = false;
        std::atomic<bool> clientConnected = false;

        std::mutex writeQueueMutex;
        std::queue<std::vector<char>> writeQueue;
        std::mutex readQueueMutex;
        std::queue<std::vector<char>> readQueue;

        #ifdef _WIN32
        HANDLE stopEvent = INVALID_HANDLE_VALUE;
        HANDLE hPipe = INVALID_HANDLE_VALUE;
        LPTSTR lpszPipename = nullptr;
        #endif

        void connectFunc();
        void readFunc();
        void writeFunc();
        void emit(std::string event);
    };
}
#endif