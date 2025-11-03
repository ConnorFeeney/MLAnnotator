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
        this->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        const char* prefix = "\\\\.\\pipe\\";
        size_t prefixLen = std::strlen(prefix);
        size_t nameLen = std::strlen(pipeName);
        size_t totalLen = prefixLen + nameLen + 1;
        
        this->lpszPipename = new char[totalLen];
        std::memcpy(this->lpszPipename, prefix, prefixLen);
        std::memcpy(this->lpszPipename + prefixLen, pipeName, nameLen);
        this->lpszPipename[totalLen - 1] = '\0';

        this->hPipe = CreateNamedPipeA(
            this->lpszPipename, 
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 
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
        #endif

        this->pipeRunning = true;
        std::cout << "Pipe Created" << std::endl;

        this->connectThread = std::make_unique<std::thread>([this]() {
            this->connectFunc();
        });
    }
    
    Pipe::~Pipe() {
        this->pipeRunning = false;
        #ifdef _WIN32
        if (this->stopEvent) {
            SetEvent(this->stopEvent);
        }
        #endif

        if (this->connectThread && this->connectThread->joinable()) {
            this->connectThread->join();
            this->connectThread.reset();
        }

        if (this->readThread && this->readThread->joinable()) {
            this->readThread->join();
            this->readThread.reset();
        }

        if (this->writeThread && this->writeThread->joinable()) {
            this->writeThread->join();
            this->writeThread.reset();
        }

        #ifdef _WIN32
        if (this->stopEvent) {
            CloseHandle(this->stopEvent);
            this->stopEvent = INVALID_HANDLE_VALUE;
        }

        DisconnectNamedPipe(this->hPipe); 
        CloseHandle(this->hPipe); 
        this->hPipe = INVALID_HANDLE_VALUE;

        delete[] lpszPipename;
        lpszPipename = nullptr;
        #endif

        std::cout << "Pipe Closed" << std::endl;
    }

    void Pipe::connectFunc() {
        #ifdef _WIN32
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset event

        if (!overlapped.hEvent) {
            std::cerr << "Failed to create event for ConnectNamedPipe" << std::endl;
            return;
        }

        std::cout << "Awaiting Connection..." << std::endl;
        BOOL connected = ConnectNamedPipe(this->hPipe, &overlapped);
        if (connected) {
            std::cout << "Client connected immediately." << std::endl;
            this->clientConnected = true;
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                HANDLE events[2] = { overlapped.hEvent, this->stopEvent };
                DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

                if (waitResult == WAIT_OBJECT_0) {
                    std::cout << "Client connected!" << std::endl;
                    this->clientConnected = true;
                } else if (waitResult == WAIT_OBJECT_0 + 1) {
                    // Stop event triggered
                    std::cout << "Stopping pipe wait..." << std::endl;
                    CancelIo(this->hPipe);
                }
            } else if (err == ERROR_PIPE_CONNECTED) {
                std::cout << "Client already connected!" << std::endl;
                this->clientConnected = true;
            } else {
                std::cerr << "ConnectNamedPipe failed: " << err << std::endl;
            }
        }

        CloseHandle(overlapped.hEvent);
        #endif

        if (this->clientConnected) {
            this->readThread = std::make_unique<std::thread>([this]() {
                this->readFunc();
            });

            this->writeThread = std::make_unique<std::thread>([this]() {
                this->writeFunc();
            });
        }
    }

    void Pipe::readFunc() {
        bool readingHeader = true;
        uint32_t msgSize = 0;
        char headerBuf[sizeof(uint32_t)] = {0};
        std::vector<char> messageBuf;

        #ifdef _WIN32
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset event
        if (!overlapped.hEvent) {
            std::cerr << "Failed to create event for async read" << std::endl;
            return;
        }
        #endif

        while (this->pipeRunning && this->clientConnected) {
            char* buffer = nullptr;
            size_t bytesToRead = 0;

            if (readingHeader) {
                buffer = headerBuf;
                bytesToRead = sizeof(uint32_t);
            } else {
                buffer = messageBuf.data();
                bytesToRead = msgSize;
            }

            // --- Platform-specific async read ---
            size_t bytesRead = 0;
            #ifdef _WIN32
            DWORD dwBytesRead = 0;
            BOOL success = ReadFile(
                this->hPipe,
                buffer,
                static_cast<DWORD>(bytesToRead),
                &dwBytesRead,
                &overlapped
            );

            if (!success) {
                DWORD err = GetLastError();

                if (err == ERROR_IO_PENDING) {
                    HANDLE events[2] = { overlapped.hEvent, this->stopEvent };
                    DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

                    if (waitResult == WAIT_OBJECT_0 + 1) {
                        CancelIo(this->hPipe);
                        break;
                    }

                    if (!GetOverlappedResult(this->hPipe, &overlapped, &dwBytesRead, FALSE)) {
                        std::cerr << "GetOverlappedResult failed: " << GetLastError() << std::endl;
                        break;
                    }
                } 
                else if (err == ERROR_BROKEN_PIPE) {
                    std::cout << "Client disconnected." << std::endl;
                    this->clientConnected = false;
                    break;
                } 
                else {
                    std::cerr << "ReadFile failed: " << err << std::endl;
                    break;
                }
            }
            bytesRead = static_cast<size_t>(dwBytesRead);
            ResetEvent(overlapped.hEvent);
            #endif

            if (bytesRead == 0) {
                std::cout << "Client closed pipe." << std::endl;
                this->clientConnected = false;
                break;
            }

            if (readingHeader) {
                std::memcpy(&msgSize, headerBuf, sizeof(uint32_t));

                if (msgSize == 0) {
                    std::cerr << "Invalid message size: " << msgSize << std::endl;
                    break;
                }

                messageBuf.resize(msgSize);
                readingHeader = false;
            } else {
                {
                    std::lock_guard<std::mutex> lock(this->readQueueMutex);
                    this->readQueue.push(std::move(messageBuf));
                }
                
                messageBuf = std::vector<char>();
                readingHeader = true;

                this->emit("read");
            }
        }

        #ifdef _WIN32
        CloseHandle(overlapped.hEvent);
        #endif
    }

    void Pipe::writeFunc() {
        #ifdef _WIN32
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset
        if (!overlapped.hEvent) {
            std::cerr << "Failed to create event for WriteFile" << std::endl;
            return;
        }
        #endif

        while (this->pipeRunning && this->clientConnected) {
            std::vector<char> msg;
            {
                std::lock_guard<std::mutex> lock(writeQueueMutex);
                if(writeQueue.empty()) {
                    continue;
                }

                msg = std::move(writeQueue.front());
                writeQueue.pop();
            }

            uint32_t msgSize = static_cast<uint32_t>(msg.size());
            std::vector<char> writeBuf(sizeof(uint32_t) + msgSize);
            std::memcpy(writeBuf.data(), &msgSize, sizeof(uint32_t));
            std::memcpy(writeBuf.data() + sizeof(uint32_t), msg.data(), msgSize);

            size_t bytesToWrite = writeBuf.size();
            char* buffer = writeBuf.data();

            #ifdef _WIN32
            DWORD bytesWritten = 0;
            BOOL success = WriteFile(
                this->hPipe, 
                buffer, 
                static_cast<DWORD>(bytesToWrite), 
                &bytesWritten, 
                &overlapped
            );

            if (!success) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    HANDLE events[2] = { overlapped.hEvent, stopEvent };
                    DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

                    if (waitResult == WAIT_OBJECT_0 + 1) {
                        CancelIo(this->hPipe);
                        break;
                    }

                    if (!GetOverlappedResult(this->hPipe, &overlapped, &bytesWritten, FALSE)) {
                        std::cerr << "GetOverlappedResult failed: " << GetLastError() << std::endl;
                        break;
                    }
                } else {
                    std::cerr << "WriteFile failed: " << err << std::endl;
                    break;
                }
            }
            #endif
        }

        #ifdef _WIN32
        CloseHandle(overlapped.hEvent);
        #endif
    }


    int Pipe::on(std::string event, std::function<void(const char*, size_t)> handler) {
        std::lock_guard<std::mutex> lock(this->eventMutex);
        if (EVENTS.find(event) == EVENTS.end()) {
            std::cerr << "ERR::C::Event not allowed" << std::endl;
            return -1;
        }

        this->eventHandlers[event].push_back(handler);
        int id = this->eventHandlers[event].size();

        return id;
    }

    int Pipe::removeListener(std::string event, int id) {
        std::lock_guard<std::mutex> lock(this->eventMutex);
        if (EVENTS.find(event) == EVENTS.end()) {
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
        if (EVENTS.find(event) == EVENTS.end()) {
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