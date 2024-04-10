#pragma once
#include <vector>
#include "Platform.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
#    include <WinSock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "WS2_32.lib")
#    include <Windows.h>
#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
	typedef int NoBiggySocket;
#	define NoBiggyAcceptSocketError -1 
#elif defined(PLATFORM_WINDOWS)
	typedef SOCKET NoBiggySocket;
#	define NoBiggyAcceptSocketError INVALID_SOCKET
#endif

namespace Server {

	struct Lobby {
		std::string ID_Lobby;
		NoBiggySocket peer1;
		NoBiggySocket peer2;
	};

	inline const int UUID_LENGTH = 6;
	inline const int MAX_WORKERS = 10;
	inline const int PORT = 3000;
	inline const int BACKLOG = 10;

	void run();
	bool startServer();

	void runWorker();

	void reduceActiveConexions();
	void incrementActiveConexions();

	void onError(NoBiggySocket socket, bool closeSocket, const char* errorMessage);
	bool checkForErrors(NoBiggySocket socket, int errorMacro, const char* errorMessage, bool closeSocket = false);

	void setTSQueue(NoBiggySocket socket);
	NoBiggySocket getTSQueue();

	std::string generateNewUUID();

	inline NoBiggySocket SERVER_SOCKET;

	inline std::vector<std::thread> WORKERS;
	inline std::queue<NoBiggySocket> SOCKET_QUEUE;
	inline int activeConnexions = 0;

	inline std::mutex activeConnectionsMutex;
	inline std::mutex queueMutex;
	inline std::condition_variable queueCondition;
	inline std::unordered_map<std::string, Lobby> LOBBIES;

	inline bool keepRunning = true;
}