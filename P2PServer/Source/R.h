#pragma once


#include <cstdint>
#include <queue>
#include <mutex>
#include <unordered_map>

#include <iostream>
#include <stdio.h>
#include <string.h>

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


#include <stdio.h>

#ifdef DISABLE_LOGGING
#    define RLog(msg, ...) ()

#else
#    define RLog(msg, ...) printf(msg, ##__VA_ARGS__)

#endif

#define BIND_FN(fn)                                             \
    [this](auto&&... args) -> decltype(auto) {                  \
        return this->fn(std::forward<decltype(args)>(args)...); \
    }


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

        template <typename T>
        bool inBoundOffset(std::size_t const offset) {
            if (offset + sizeof(T) >= maxSize || offset < 0) {
                RLog("[Buffer] Can't access out of bounds");
                return false;
            }
            return true;
        }

        // -- Methods
        template <typename T>
        T read(std::size_t const offset) {
            if (offset + sizeof(T) >= maxSize || offset < 0)
                RLog("[Buffer] Can't access out of bounds");

            T value;
            memcpy(&value, ini + offset, sizeof(T));
            return value;
        }

        // expects real values such as uint8_t and not pointers
        template <typename T>
        void write(T const value) {
            this->write(&value, sizeof(T));
        }

        // expects pointers to values
        template <typename T>
        void write(T const value, int appendLength) {
            increaseBufferSizeIfNecessary(appendLength);

            memcpy(ini + size, value, appendLength);
            size += appendLength;
        }

        void increaseBufferSizeIfNecessary(int appendLength) {
            if (appendLength + size >= maxSize) {
                // allocate new & bigger memory
                maxSize = (appendLength + size) * 2;
                char *newBuffer = new char[maxSize];
                memcpy(newBuffer, ini, size);

                delete[] ini;
                ini = newBuffer;
            }
        }
    };
}  // namespace R

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <execinfo.h>
#    include <fcntl.h>
#    include <netdb.h>
#elif defined(PLATFORM_WINDOWS)
#    include <WinSock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "WS2_32.lib")
#    include <Windows.h>
#    include <io.h>
#endif


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
                return NULL;
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

    template <class ADAPTER>
    const auto &getQueueCObject(ADAPTER &a) {
        struct hack : private ADAPTER {
            static auto &get(ADAPTER &a) {
                return a.*(&hack::c);
            }
        };

        return hack::get(a);
    }

    template <typename T>
    void removeFromVector(std::vector<T> &vector, T value) {
        auto it = std::find(vector.begin(), vector.end(), value);
        if (it != vector.end()) {
            vector.erase(it);
        }
    }

    template <typename T, typename K>
    inline bool keyExistsInMap(std::unordered_map<T, K> &map, T &key) {
        auto it = map.find(key);
        if (it == map.end()) {
            return false;
        }
        return true;
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

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

    inline void onExceptionHandler(int sig) {
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        RLog("[Expception Handler]: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }

    inline void stackTracing() {
        signal(SIGSEGV, onExceptionHandler);
    }

    inline void avoidSigPipe() {
        signal(SIGPIPE, SIG_IGN);
    }

#elif defined(PLATFORM_WINDOWS)

    // make it multipplatform but useless
    inline void stackTracing() {}
    inline void avoidSigPipe() {}

#endif

    inline void hexDump(Buffer buffer) {
        unsigned char *buf = (unsigned char *)buffer.ini;
        unsigned int i, j;
        for (i = 0; i < buffer.size; i += 16) {
            RLog("%06x: ", i);
            for (j = 0; j < 16; j++)
                if (i + j < buffer.size)
                    RLog("%02x ", buf[i + j]);
                else
                    RLog("   ");
            RLog(" ");
            for (j = 0; j < 16; j++)
                if (i + j < buffer.size)
                    RLog("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
            RLog("\n");
        }
    }
}  // namespace R::Utils

#include <iostream>

namespace R::Net {

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    typedef int Socket;
#    define readSocket(socket, buffer, bufferSize) read(socket, buffer, bufferSize)
#    define SocketError -1
#elif defined(PLATFORM_WINDOWS)
    typedef SOCKET Socket;
#    define readSocket(socket, buffer, bufferSize) _read(socket, buffer, bufferSize)
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
        RLog("%s - errno %i\n", errorMessage, errno);
        if (closeSocket) {
            close(socket);
        }
    }

#elif defined(PLATFORM_WINDOWS)

    inline uint32_t getRTTOfClient(Socket _socket) {
        // TODO how to get RTT in windows
        return 0;
    }

    inline bool setServerNonBlockingMode(Socket socket) {
        unsigned long mode = 1;
        return (ioctlsocket(socket, FIONBIO, &mode) == 0);
    }

    inline void onError(Socket socket, bool closeSocket, const char *errorMessage) {
        if (closeSocket) {
            closesocket(socket);
        }
        RLog("%s --- winsock2 error code is: %i\n", errorMessage, WSAGetLastError());
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
        int bufferSize = readSocket(socket, stackBuffer, 255);
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

#include <string>

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

        static std::shared_ptr<Client> makeAndSet(Socket socket) {
            auto client = make();
            client->_socket = socket;
            client->isRunning = true;
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

            RLog("[Client] Connected to hostname %s and port %i\n", hostname, port);
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

            iResult = getaddrinfo(hostname, std::to_string(port).c_str(), &hints, &result);
            if (iResult != 0) {
                onError(_socket, false, "[Client] Error on getaddrinfo");
                return false;
            }

            ptr = result;
            Socket ConnectSocket = INVALID_SOCKET;

            for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
                ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                if (checkForErrors(ConnectSocket, INVALID_SOCKET, "[Client] Error on socket creation", false)) {
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
            if (checkForErrors(ConnectSocket, INVALID_SOCKET, "[Client] Couldn't connect to the server", true)) {
                return false;
            }

            RLog("[Client] Connected to hostname %s and port %i\n", hostname, port);
            isRunning = true;
            return true;
        }

#endif

        inline void terminate() {
            onError(_socket, true, "[Client] Closing the client socket!");
        }

        inline int sendMessage(Buffer buff) {
            auto sendResponse = Net::sendMessage(_socket, buff, "[Client] Couldn't send message");
            if (sendResponse == -1) {
                isRunning = false;
            }
            return sendResponse;
        }

        inline Buffer readMessage() {
            auto readResponse = Net::readMessage(_socket, "[Client] Couldn't read message");
            if (readResponse.size <= 0) {
                isRunning = false;
            }
            return readResponse;
        }
    };

}  // namespace R::Net

#include <cstdint>

namespace R::Net::P2P {

    inline const int MAX_PACKAGE_LENGTH = 40;
    inline const int SECURITY_HEADER_LENGTH = 23;
    inline const char* SECURITY_HEADER = "0sdFGeVi3ItN1qwsHp3mcDF";
    inline const int UUID_LENGTH = 5;

    enum ClientClientHeaderFlags {
        // Type of the action we are trying peer-message
        ClientClientHeaderFlags_Bit1 = 1 << 7,  // 10000000
    };

    // Client-Server data flags
    enum ClientServerHeaderFlags {
        // Type of the lobby public/private
        ClientServerHeaderFlags_Public = 1 << 5,  // 00100000
        // Type of the action we are trying create/connect/disconnect/peersConnectSuccess
        ClientServerHeaderFlags_Bit1 = 1 << 7,  // 10000000
        ClientServerHeaderFlags_Bit2 = 1 << 6,  // 01000000
    };

    // Server-Client data flags
    enum ServerClientHeaderFlags {
        // action type send-uuid/connect
        ServerClientHeaderFlags_Bit1 = 1 << 7,  // 10000000

    };

    enum class LobbyPrivacyType {
        Private,
        Public
    };

    enum class ClientActionType {
        Create,
        Connect,
        Disconnect,
        PeerConnectSuccess
    };

    enum ClientClientActionType {
        PeerMessage
    };

    enum ServerActionType {
        Connect,
        SendUUID,
    };

    inline bool isValidAuthedRequest(Buffer& buffer) {
        return Utils::isInRange(buffer.size, SECURITY_HEADER_LENGTH + 1, MAX_PACKAGE_LENGTH) && strncmp(buffer.ini, SECURITY_HEADER, SECURITY_HEADER_LENGTH) == 0;
    }

    inline Buffer createSecuredBuffer() {
        auto buffer = Buffer(SECURITY_HEADER_LENGTH);

        buffer.write(SECURITY_HEADER, SECURITY_HEADER_LENGTH);

        return buffer;
    }

    inline uint8_t getProtocolHeader(Buffer& buffer) {
        return buffer.ini[SECURITY_HEADER_LENGTH];
    }

    inline Buffer getPayload(Buffer& buffer) {
        auto payloadBuffer = Buffer(buffer.size);
        auto headerSize = SECURITY_HEADER_LENGTH + 1;

        payloadBuffer.write(buffer.ini + headerSize, buffer.size - headerSize);

        return payloadBuffer;
    }

    // start Client section

    inline uint8_t createClientProtocolHeader(LobbyPrivacyType lobbyType, ClientActionType clientActionType) {
        uint8_t headerFlags = 0;
        if (lobbyType == LobbyPrivacyType::Public) {
            R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Public);
        }
        switch (clientActionType) {
            case ClientActionType::Connect:  // 11
                R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Bit1);
                R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Bit2);
                break;
            case ClientActionType::Create:  // 10
                R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Bit1);
                break;
            case ClientActionType::Disconnect:  // 01
                R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Bit2);
                break;
            case ClientActionType::PeerConnectSuccess:  // 00
                break;
        }

        return headerFlags;
    }

    inline Buffer createClientBuffer(LobbyPrivacyType lobbyType, ClientActionType action) {
        auto buffer = createSecuredBuffer();
        auto headerFlags = createClientProtocolHeader(lobbyType, action);

        buffer.write(headerFlags);

        return buffer;
    }

    inline Buffer createClientPeersConnectSuccessBuffer() {
        return createClientBuffer(LobbyPrivacyType::Private, ClientActionType::PeerConnectSuccess);
    }

    inline Buffer createClientDisconnectBuffer() {
        return createClientBuffer(LobbyPrivacyType::Private, ClientActionType::Disconnect);
    }

    inline Buffer createClientCreateLobbyBuffer(LobbyPrivacyType privacyType, uint16_t clientPort) {
        auto buffer = createClientBuffer(privacyType, ClientActionType::Create);

        buffer.write(htons(clientPort));

        return buffer;
    }

    inline Buffer createClientPublicConnectBuffer(uint16_t clientPort) {
        auto buffer = createClientBuffer(LobbyPrivacyType::Public, ClientActionType::Connect);

        buffer.write(htons(clientPort));

        return buffer;
    }

    inline Buffer createClientPrivateConnectBuffer(std::string& uuid, uint16_t clientPort) {
        auto buffer = createClientBuffer(LobbyPrivacyType::Private, ClientActionType::Connect);

        buffer.write(htons(clientPort));
        buffer.write(uuid.c_str(), UUID_LENGTH);

        return buffer;
    }

    inline ClientActionType getClientActionTypeFromHeaderByte(uint8_t headerByte) {
        bool isBit1Set = R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit1);
        bool isBit2Set = R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit2);

        if (isBit1Set) {
            if (isBit2Set) {
                return ClientActionType::Connect;  // 11 = connect
            } else {
                return ClientActionType::Create;  // 10 = createLobby
            }
        } else {
            if (isBit2Set) {
                return ClientActionType::Disconnect;  // 01 = disconnect
            } else {
                return ClientActionType::PeerConnectSuccess;  // 00 = peersConnectSuccess
            }
        }
    }

    inline LobbyPrivacyType getLobbyPrivacyTypeFromHeaderByte(uint8_t headerByte) {
        if (R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Public)) {
            return LobbyPrivacyType::Public;
        }
        return LobbyPrivacyType::Private;
    }

    // end Client secion

    // start Server secion

    struct ServerConnectPayload {
        in_addr ipAddress;
        uint16_t port;
        uint32_t delay;

        void Print() {
            char tempBuff[INET_ADDRSTRLEN];

            RLog("\nStart peer info ---- \n\n");
            RLog("Peer Port: %i\n", this->port);
            RLog("Peer IP Address: %s\n", inet_ntop(AF_INET, &this->ipAddress, tempBuff, INET_ADDRSTRLEN));
            RLog("Peer delay: %i\n", this->delay);
            RLog("\nEnd peer info   ---- \n\n");
        }
    };

    inline uint8_t createServerProtocolHeader(ServerActionType serverActionType) {
        uint8_t headerFlags = 0;
        if (serverActionType == ServerActionType::Connect) {  // 1
            R::Utils::setFlag(headerFlags, ClientServerHeaderFlags_Bit1);
        }

        return headerFlags;
    }

    inline Buffer createServerConnectBuffer(uint32_t ipAddress, uint16_t port, uint32_t delay) {
        auto buffer = createSecuredBuffer();
        auto headerFlags = createServerProtocolHeader(ServerActionType::Connect);

        buffer.write(headerFlags);
        buffer.write(ipAddress);
        buffer.write(htons(port));
        buffer.write(htonl(delay));

        return buffer;
    }

    inline Buffer createServerSendUUIDBuffer(std::string& uuid) {
        auto buffer = createSecuredBuffer();
        auto headerFlags = createServerProtocolHeader(ServerActionType::SendUUID);

        buffer.write(headerFlags);
        buffer.write(uuid.c_str(), UUID_LENGTH);

        return buffer;
    }

    inline ServerActionType getServerActionTypeFromHeaderByte(uint8_t headerByte) {
        if (R::Utils::isFlagSet(headerByte, ServerClientHeaderFlags::ServerClientHeaderFlags_Bit1)) {
            return ServerActionType::Connect;
        }
        return ServerActionType::SendUUID;
    }

    inline std::string getUUIDFromSendUUIDBuffer(Buffer& buffer) {
        auto protocolHeader = getProtocolHeader(buffer);
        auto actionType = getServerActionTypeFromHeaderByte(protocolHeader);

        if (actionType != ServerActionType::SendUUID)
            return "";

        auto payload = getPayload(buffer);
        return std::string(payload.ini, UUID_LENGTH);
    }

    inline ServerConnectPayload getPayloadFromServerConnectBuffer(Buffer& buffer) {
        auto protocolHeader = getProtocolHeader(buffer);
        auto actionType = getServerActionTypeFromHeaderByte(protocolHeader);

        if (actionType != ServerActionType::Connect) {
            return {0, 0, 0};  // empty/error
        }

        auto payload = getPayload(buffer);

        auto ipAddress = payload.read<in_addr>(0);
        auto port = payload.read<uint16_t>(4);
        auto delay = payload.read<uint32_t>(6);

        return {ipAddress, ntohs(port), ntohl(delay)};
    }

    // end Server secion

    // start Client Client section

    inline ClientClientActionType getClientClientProtocolHeader(uint8_t headerFlags) {
        return ClientClientActionType::PeerMessage;
    }

    inline uint8_t createClientClientProtocolHeader(ClientClientActionType actionType) {
        uint8_t headerFlags = 0;
        if (actionType == ClientClientActionType::PeerMessage) {
            R::Utils::setFlag(headerFlags, ClientClientHeaderFlags::ClientClientHeaderFlags_Bit1);
        }

        return headerFlags;
    }

    inline Buffer createClientPeerMessageBuffer() {
        auto buffer = createSecuredBuffer();
        auto headerFlags = createClientClientProtocolHeader(ClientClientActionType::PeerMessage);

        buffer.write(headerFlags);

        return buffer;
    }

    // end Client Client section

}  // namespace R::Net::P2P

#include <chrono>
#include <vector>
#include <functional>
#include <thread>

namespace R::Net::P2P {

    typedef std::function<Socket()> LogCallbackFunction;
    inline const int defaultTimerInSeconds = 10;
    inline uint8_t KeepAliveHeader = 255;

    class KeepAliveManager {
       public:
        KeepAliveManager(int _timerInSeconds)
            : timerInSeconds(_timerInSeconds) {}
        KeepAliveManager()
            : timerInSeconds(defaultTimerInSeconds) {}

        static inline std::shared_ptr<KeepAliveManager> make(int _timerInSeconds = defaultTimerInSeconds) {
            return std::make_shared<KeepAliveManager>(_timerInSeconds);
        }

        static inline std::shared_ptr<KeepAliveManager> makeAndRun(int _timerInSeconds = defaultTimerInSeconds) {
            auto instance = make(_timerInSeconds);
            instance->runInNewThread();
            return instance;
        }

        static inline bool isKeepAlivePackage(Buffer& buffer) {
            return isValidAuthedRequest(buffer) && getProtocolHeader(buffer) == KeepAliveHeader;
        }

        static inline int sendKeepAlivePackage(Socket socket) {
            auto buffer = createSecuredBuffer();
            buffer.write(KeepAliveHeader);

            return sendKeepAlivePackage(socket, buffer);
        }

        static inline int sendKeepAlivePackage(Socket socket, Buffer& buffer) {
            return Net::sendMessage(socket, buffer, "[Keep Alive] Client socket disconected!");
        }

        static inline bool isSocketActive(Socket socket) {
            auto sendResponse = sendKeepAlivePackage(socket);
            if (sendResponse == -1) {
                return false;
            }
            return true;
        }

        inline void run() {
            int sendResponse = 0;
            auto buffer = createSecuredBuffer();
            buffer.write(KeepAliveHeader);

            while (keepRunning) {
                std::this_thread::sleep_for(std::chrono::seconds(timerInSeconds));
                for (auto& socket : keepAliveSockets) {
                    sendResponse = sendKeepAlivePackage(socket, buffer);
                    if (sendResponse != -1) {
                        continue;
                    }

                    removeSocketToKeepAlive(socket);
                    if (onClosedCallback != nullptr) {
                        onClosedCallback(socket);
                    }
                }
            }
        }

        inline void runInNewThread() {
            runningThread = std::thread(BIND_FN(KeepAliveManager::run));
        }

        inline void addNewSocketToKeepAlive(Socket _socket) {
            keepAliveSockets.push_back(_socket);
        }

        inline void removeSocketToKeepAlive(Socket _socket) {
            Utils::removeFromVector(keepAliveSockets, _socket);
        }

        inline void addOnConnectionClosedCallback(std::function<void(Socket)> func) {
            onClosedCallback = func;
        }

        std::vector<Socket> keepAliveSockets;
        std::function<void(Socket)> onClosedCallback = nullptr;
        std::thread runningThread;

        // flag to be able to stop it
        bool keepRunning = true;
        int timerInSeconds;
    };
}  // namespace R::Net::P2P

namespace R::Net {

    struct AcceptResponseWithIp {
        Socket socket;
        in_addr ipAddress;
    };

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

            RLog("[Server] Started listening in port %i\n", port);
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

            RLog("[Server] Started listening in port %i\n", port);
            isRunning = true;
            return true;
        }

#endif

        inline void terminate() {
            onError(_socket, true, "[Server] Closing the server socket!");
        }

        inline AcceptResponseWithIp acceptNewConnection(bool checkErrors = true) {
            struct sockaddr_in clientAddress;
            socklen_t addressLength = sizeof(sockaddr_in);

            if (!isRunning) {
                RLog("[Server] Cannot accept connections if server is not running");
                return {(Socket)-1};
            }

            Socket AcceptSocket = accept(_socket, (struct sockaddr *)&clientAddress, &addressLength);
            if (checkErrors && checkForErrors(AcceptSocket, SocketError, "[Server] Error while accepting new connections", true))
                return {(Socket) - 1};

            return {AcceptSocket, clientAddress.sin_addr};
        }

        inline int sendMessage(Socket socket, Buffer buff) {
            return Net::sendMessage(socket, buff, "[Server] Couldn't send message");
        }

        inline Buffer readMessage(Socket socket) {
            return Net::readMessage(socket, "[Server] Couldn't read message");
        }
    };

}  // namespace R::Net