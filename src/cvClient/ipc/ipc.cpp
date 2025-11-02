#include "ipc.h"

#include <iostream>
#include <set>

#include <cstring>

const std::set<std::string> EVENTS = {
    "connect",
    "data",
    "read",
    "write"
};

namespace ipc {
    Pipe::Pipe(const char* pipeName) {
#ifdef _WIN32
        const char* prefix = "\\\\.\\pipe\\";
        size_t prefixLen = std::strlen(prefix);
        size_t nameLen = std::strlen(pipeName);
        size_t totalLen = prefixLen + nameLen + 1; // +1 for null terminator
        
        this->lpszPipename = new char[totalLen];
        std::memcpy(this->lpszPipename, prefix, prefixLen);
        std::memcpy(this->lpszPipename + prefixLen, pipeName, nameLen);
        this->lpszPipename[totalLen - 1] = '\0'; // null terminate

        this->hPipe = CreateNamedPipeA(
            this->lpszPipename, 
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, 
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 
            1, 
            4096, 
            4096, 
            0, 
            NULL
        );

        if (this->hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "ERR::WIN32::(Pipe failed to start)" << std::endl;
        }

        std::cout << "Pipe Created" << std::endl;

        this->connectThread = std::make_unique<std::thread>([this]() {
            this->connectFunc();
        });
#endif
    }
    
    Pipe::~Pipe() {
#ifdef _WIN32
        delete[] lpszPipename;
        lpszPipename = nullptr;

        if (this->connectThread && this->connectThread->joinable()) {
            this->connectThread->join();
            this->connectThread.reset();
        }

        DisconnectNamedPipe(this->hPipe); 
        CloseHandle(this->hPipe); 
        std::cout << "Pipe Closed" << std::endl;
#endif
    }

    void Pipe::connectFunc() {
        std::cout << "Awaiting Connection" << std::endl;
    }

    int Pipe::on(std::string event, std::function<void(const char*, size_t)> handler) {
        std::lock_guard<std::mutex> lock(this->eventMutex);
        bool eventAllowed = false;
        for (const std::string& eventName : EVENTS) {
            if (event == eventName) {
                eventAllowed = true;
                break;
            }
        }
        if (!eventAllowed) {
            std::cerr << "ERR::C::Event not allowed" << std::endl;
            return -1;
        }

        this->eventHandlers[event].push_back(handler);
        int id = this->eventHandlers[event].size();

        return id;
    }

    int Pipe::removeListener(std::string event, int id) {
        std::lock_guard<std::mutex> lock(this->eventMutex);
        bool eventAllowed = false;
        for (const std::string& eventName : EVENTS) {
            if (event == eventName) {
                eventAllowed = true;
                break;
            }
        }
        if (!eventAllowed) {
            std::cerr << "ERR::C::Event not allowed" << std::endl;
            return -1;
        }

        auto it = this->eventHandlers[event].begin() + id - 1;
        if (it >= this->eventHandlers[event].begin() && it < this->eventHandlers[event].end()) {
            this->eventHandlers[event].erase(it);
            return id;
        } else {
            std::cerr << "ERR::C::Listener id out of range" << std::endl;
            return -1;
        }
    }

    int Pipe::removeAllListeners(std::string event) {
        std::lock_guard<std::mutex> lock(this->eventMutex);
        bool eventAllowed = false;
        for (const std::string& eventName : EVENTS) {
            if (event == eventName) {
                eventAllowed = true;
                break;
            }
        }
        if (!eventAllowed) {
            std::cerr << "ERR::C::Event not allowed" << std::endl;
            return -1;
        }
        
        this->eventHandlers.erase(event);

        return 0;
    }

    int Pipe::read(const char* buffer, size_t size) {
        return 0;
    }

    int Pipe::write(const char* data, size_t size) {
        return 0;
    }
}