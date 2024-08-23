#pragma once


#include <cstdint>
#include <queue>
#include <execinfo.h>
#include <unistd.h>

namespace R::Utils {

    inline bool isInRange(int value, int lowRange, int highRange) {
        return value <= highRange && value >= lowRange;
    }

    inline bool isFlagSet(uint8_t flagObject, uint8_t flag) {
        return 0 != (flagObject & flag);
    }

    inline void setFlag(uint8_t &flagObject, uint8_t flag) {
        flagObject |= flag;
    }

    inline void unsetFlag(uint8_t &flagObject, uint8_t flag) {
        flagObject &= ~flag;
    }

    template <typename T>
    inline T getFromQueue(std::queue<T> &queue) {
        if (queue.empty()) {
            if constexpr (std::is_same_v<T, std::string>) {
                return "";
            } else if constexpr (std::is_same_v<T, int>) {
                return -1;
            } else {
                return nullptr;
            }
        }

        auto value = queue.front();
        queue.pop();
        return value;
    }

    template <typename T>
    inline T getThreadSafeFromQueue(std::queue<T> &queue, std::mutex &queueMutex) {
        std::lock_guard<std::mutex> lock(queueMutex);
        return getFromQueue(queue);
    }

    template <typename T>
    inline T getThreadSafeFromQueue(std::queue<T> &queue, std::mutex &queueMutex, std::condition_variable &condition) {
        std::unique_lock<std::mutex> lock(queueMutex);
        condition.wait(lock);
        return getFromQueue(queue);
    }

    template <typename T>
    inline void setThreadSafeToQueue(std::queue<T> &queue, std::mutex &queueMutex, T value) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push(value);
    }

    inline std::string generateUUID(int length) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        std::string uuid;
        uuid.reserve(length);

        for (int i = 0; i < length; ++i) {
            uuid += alphanum[rand() % (sizeof(alphanum) - 1)];
        }
        return uuid;
    };

    inline int randomNumber(int min, int max) {
        return rand() % (max - min + 1) + min;
    }

    inline unsigned int randomUintNumber(int min, int max) {
        return (unsigned int)(rand() % (max - min + 1) + min);
    }

    inline void onExceptionHandler(int sig) {
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }

    inline void stackTracing() {
        signal(SIGSEGV, onExceptionHandler);
    }
}  // namespace R::Utils
#include <iostream>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    pragma message("WIN32 || _WIN32 || __WIN32__ || __NT__")
#    ifndef PLATFORM_WINDOWS
#        define PLATFORM_WINDOWS
#    endif
#    ifdef _WIN64
#    endif

#elif __APPLE__
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
#    elif TARGET_OS_MACCATALYST
#    elif TARGET_OS_IPHONE
#    elif TARGET_OS_MAC
#        ifndef PLATFORM_MACOS
#            define PLATFORM_MACOS
#        endif
#    else
#        error "Unknown Apple platform"
#    endif

#elif __ANDROID__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif __linux__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif __unix__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif defined(_POSIX_VERSION)
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#else
#    error("Unknown compiler")
#endif

#ifdef PLATFORM_LINUX
#    pragma message("This is linux")
#elif defined(PLATFORM_MACOS)
#    pragma message("This is MacOS")
#elif defined(PLATFORM_WINDOWS)
#    pragma message("This is Windows")
#else
#    pragma message("This is an unknown OS")
#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <fcntl.h>
#    include <netdb.h>
#elif defined(PLATFORM_WINDOWS)
#    include <WinSock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "WS2_32.lib")
#    include <Windows.h>
#endif


#include <iostream>
#include <stdio.h>
#include <string.h>
namespace R {
    class Buffer {
       public:
        char *ini;
        size_t size = 0;
        size_t maxSize = 0;

        // destructor
        ~Buffer() {
            delete[] ini;
        }

        // Constructor
        explicit Buffer(int n)
            : ini(new char[n]{0}), maxSize(n) {}

        // Copy Constructor
        Buffer(const Buffer &otherBuff) {
            ini = new char[otherBuff.maxSize];
            size = otherBuff.size;
            maxSize = otherBuff.maxSize;

            memcpy(ini, otherBuff.ini, otherBuff.size);
        }

        uint8_t operator[](int position) {
            return ini[position];
        }

        // Copy assignment
        Buffer &operator=(const Buffer &otherBuff) {
            if (this == &otherBuff)
                return *this;

            delete[] ini;

            ini = new char[otherBuff.maxSize];
            size = otherBuff.size;
            maxSize = otherBuff.maxSize;

            memcpy(ini, otherBuff.ini, otherBuff.size);
            return *this;
        }

        // Move Constructor
        Buffer(Buffer &&otherBuff) {
            ini = otherBuff.ini;
            size = otherBuff.size;
            maxSize = otherBuff.maxSize;

            otherBuff.ini = nullptr;
        }

        // Move Assignment
        Buffer &operator=(Buffer &&other_bfr) {
            ini = other_bfr.ini;
            size = other_bfr.size;
            maxSize = other_bfr.maxSize;

            other_bfr.ini = nullptr;

            return *this;
        }

        // -- Methods
        template <typename T>
        T read(std::size_t const offset) {
            if (offset + sizeof(T) >= maxSize || offset < 0)
                printf("[Buffer] Can't read out of bounds");

            return static_cast<T>(ini[offset]);
        }

        template <typename T>
        void write(T const value) {
            this->write(value, sizeof(T));
        }

        // specialization for char*
        void write(const char *value, int appendLength) {
            increaseBufferSizeIfNecessary(appendLength);

            memcpy(ini + size, value, appendLength);
            size += appendLength;
        }

        template <typename T>
        void write(T const value, int appendLength) {
            increaseBufferSizeIfNecessary(appendLength);

            memcpy(ini + size, &value, appendLength);
            size += appendLength;
        }

        void increaseBufferSizeIfNecessary(int appendLength) {
            if (appendLength + size >= maxSize) {
                // allocate new & bigger memory
                maxSize = (appendLength + size) * 2;
                char *newBuffer = new char[maxSize];

                delete[] ini;
                ini = newBuffer;
            }
        }
    };
}  // namespace R

namespace R::Net {

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    typedef int Socket;
#    define SocketError -1
#elif defined(PLATFORM_WINDOWS)
    typedef SOCKET Socket;
#    define SocketError INVALID_SOCKET
#endif

#if defined(PLATFORM_MACOS)

    inline uint32_t getRTTOfClient(Socket _socket) {
        struct tcp_connection_info info;
        socklen_t info_len = sizeof(info);

        int result = getsockopt(_socket, IPPROTO_TCP, TCP_CONNECTION_INFO, &info, &info_len);

        return info.tcpi_srtt;
    }

#elif defined(PLATFORM_LINUX)

    inline uint32_t getRTTOfClient(Socket _socket) {
        struct tcp_info info;
        socklen_t info_len = sizeof(info);

        int result = getsockopt(_socket, IPPROTO_TCP, TCP_INFO, &info, &info_len);

        return info.tcpi_srtt;
    }

#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

    inline bool setServerNonBlockingMode(Socket socket) {
        int flags = fcntl(socket, F_GETFL, 0);
        flags = flags | O_NONBLOCK;
        return (fcntl(socket, F_SETFL, flags) == 0);
    }

    inline void onError(Socket socket, bool closeSocket, const char *errorMessage) {
        printf("%s - errno %i\n", errorMessage, errno);
        if (closeSocket) {
            close(socket);
        }
    }

#elif defined(PLATFORM_WINDOWS)

    inline uint32_t getRTTOfClient(Socket _socket) {
        // TODO how to get RTT in windows
    }

    inline void setServerNonBlockingMode(Socket socket) {
        unsigned long mode = 1;
        return (ioctlsocket(fd, FIONBIO, &mode) == 0);
    }

    inline void onError(Socket socket, bool closeSocket, const char *errorMessage) {
        if (closeSocket) {
            closesocket(socket);
        }
        printf("%s --- winsock2 error code is: %i\n", errorMessage, WSAGetLastError());
        WSACleanup();
    }
#endif

    inline int sendMessage(Socket socket, Buffer buff, const char *message) {
        auto sendResponse = send(socket, buff.ini, buff.size, 0);
        if (sendResponse < 0)
            onError(socket, false, message);

        return sendResponse;
    }

    inline Buffer readMessage(Socket socket, const char *message) {
        char stackBuffer[255];
        auto buffer = Buffer(255);
        int bufferSize = read(socket, stackBuffer, 255);
        if (bufferSize < 0) {
            onError(socket, false, message);
        } else {
            buffer.write(stackBuffer, bufferSize);
        }

        return buffer;
    }

    inline bool checkForErrors(Socket socket, int errorMacro, const char *errorMessage, bool closeSocket) {
        if (socket == errorMacro) {
            onError(socket, closeSocket, errorMessage);
            return true;
        }
        return false;
    }
}  // namespace R::Net

namespace R::Net {

    class Client {
       public:
        Socket _socket;
        bool isRunning = false;

        static std::shared_ptr<Client> make() {
            return std::make_shared<Client>();
        }

        static std::shared_ptr<Client> makeAndRun(const char *hostname, int port) {
            auto client = make();
            client->run(hostname, port);
            return client;
        }

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

        inline bool run(const char *hostname, int port) {
            struct sockaddr_in socketAddress;
            struct hostent *server;

            _socket = socket(AF_INET, SOCK_STREAM, 0);

            server = gethostbyname(hostname);
            if (server == NULL) {
                onError(_socket, false, "[Client] ERROR getting host name");
                return false;
            }

            bzero((char *)&socketAddress, sizeof(socketAddress));
            socketAddress.sin_family = AF_INET;
            bcopy((char *)server->h_addr, (char *)&socketAddress.sin_addr.s_addr, server->h_length);
            socketAddress.sin_port = htons(port);

            if (connect(_socket, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0) {
                onError(_socket, false, "[Client] Couldn't connect to the host");
                return false;
            }

            printf("[Client] Connected to hostname %s and port %i\n", hostname, port);
            isRunning = true;
            return true;
        }

#elif defined(PLATFORM_WINDOWS)

        inline bool run(const char *hostname, int port) {
            WSADATA wsaData;
            int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iResult != NO_ERROR) {
                onError(_socket, false, "[Client] Error on WSAStartup");
                return false;
            }

            struct addrinfo *result = NULL,
                            *ptr = NULL,
                            hints;

            ZeroMemory(&hints, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            iResult = getaddrinfo(hostname, port, &hints, &result);
            if (iResult != 0) {
                onError(_socket, false, "[Client] Error on getaddrinfo");
                return false;
            }

            ptr = result;
            Socket ConnectSocket = INVALID_SOCKET;

            for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
                ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                if (checkForErrors(ConnectSocket, INVALID_SOCKET, false, "[Client] Error on socket creation")) {
                    freeaddrinfo(result);
                    return false;
                }

                iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
                if (iResult == SOCKET_ERROR) {
                    closesocket(ConnectSocket);
                    ConnectSocket = INVALID_SOCKET;
                    continue;
                }
            }

            freeaddrinfo(result);
            if (checkForErrors(ConnectSocket, INVALID_SOCKET, true, "[Client] Couldn't connect to the server")) {
                return false;
            }

            printf("[Client] Connected to hostname %s and port %i\n", hostname, port);
            isRunning = true;
            return true;
        }

#endif

        inline void terminate() {
            onError(_socket, true, "[Client] Closing the client socket!");
        }

        inline int sendMessage(Buffer buff) {
            return Net::sendMessage(_socket, buff, "[Client] Couldn't send message");
        }

        inline Buffer readMessage(Socket socket) {
            return Net::readMessage(_socket, "[Client] Couldn't read message");
        }
    };

}  // namespace R

namespace R::Net {

    class Server {
       public:
        Socket _socket;
        bool isRunning = false;

        static std::shared_ptr<Server> make() {
            return std::make_shared<Server>();
        }

        static std::shared_ptr<Server> makeAndRun(int port = 3000, int backlog = 10) {
            auto server = make();
            server->run(port, backlog);
            return server;
        }

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

        inline bool run(int port = 3000, int backlog = 10) {
            _socket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddress;
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_port = htons(port);
            serverAddress.sin_addr.s_addr = INADDR_ANY;

            if (checkForErrors(bind(_socket, (sockaddr *)&serverAddress, sizeof(serverAddress)), -1, "[Server] Error on socket binding", true)) {
                return false;
            }

            if (checkForErrors(listen(_socket, backlog), -1, "[Server] Error while starting to listen on port", true)) {
                return false;
            }

            printf("[Server] Started listening in port %i\n", port);
            isRunning = true;
            return true;
        }

#elif defined(PLATFORM_WINDOWS)

        inline bool run(int port = 3000, int backlog = 10) {
            WSADATA wsaData;
            sockaddr_in service;
            service.sin_family = AF_INET;
            service.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);

            int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iResult != NO_ERROR) {
                onError(_socket, false, "");
                return false;
            }

            _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (checkForErrors(_socket, INVALID_SOCKET, "[Server] Error on socket creation", false))
                return false;

            // If error on socket binding it may mean that the port is in use, we can search a new one!
            if (checkForErrors(bind(_socket, (SOCKADDR *)&service, sizeof(service)), SOCKET_ERROR, "[Server] Error on socket binding", true))
                return false;

            if (checkForErrors(listen(_socket, 1), SOCKET_ERROR, "[Server] Error while starting to listen on port", true))
                return false;

            unsigned long blocking_mode = 0;
            if (checkForErrors(ioctlsocket(_socket, FIONBIO, &blocking_mode), -1, "[Server] Error while setting the blocking mode", true))
                return false;

            printf("[Server] Started listening in port %i\n", port);
            isRunning = true;
            return true;
        }

#endif

        inline void terminate() {
            onError(_socket, true, "[Server] Closing the server socket!");
        }

        inline Socket acceptNewConnection(bool checkErrors = true) {
            if (!isRunning) {
                printf("[Server] Cannot accept connections if server is not running");
                return -1;
            }

            Socket AcceptSocket = accept(_socket, NULL, NULL);
            if (checkErrors && checkForErrors(AcceptSocket, SocketError, "[Server] Error while accepting new connections", true))
                return -1;

            return AcceptSocket;
        }

        inline int sendMessage(Buffer buff) {
            return Net::sendMessage(_socket, buff, "[Server] Couldn't send message");
        }

        inline Buffer readMessage(Socket socket) {
            return Net::readMessage(socket, "[Server] Couldn't read message");
        }
    };

}  // namespace R::Net