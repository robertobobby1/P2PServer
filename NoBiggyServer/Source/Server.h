#pragma once
#include <vector>
#include "Platform.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>

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

namespace Server {

	inline const int MAX_WORKERS = 10;
	inline const int PORT = 3000;

	void run();
	void setup();

	void runWorker();

	void reduceActiveConexions();
	void incrementActiveConexions();

	void onError(SOCKET socket, bool closeSocket);
	bool checkForErrors(SOCKET socket, int errorMacro, bool closeSocket);

	void setTSQueue(SOCKET socket);
	SOCKET getTSQueue();

	inline SOCKET SERVER_SOCKET;

	inline std::vector<std::thread> WORKERS;
	inline std::queue<SOCKET> SOCKET_QUEUE;
	inline int activeConnexions = 0;

	inline std::mutex activeConnectionsMutex;
	inline std::mutex queueMutex;
	inline std::condition_variable queueCondition;

	inline bool keepRunning = true;

}