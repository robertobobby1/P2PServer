#include <unordered_map>
#include <mutex>
#include <thread>

#include "Tests.h"
#include "NetworkStructs.h"
#include "R.h"

namespace Tests {
    inline const int NUM_THREADS = 100;
    inline const int TEST_TIMES = 5;

    inline std::unordered_map<std::string, std::unique_ptr<std::mutex>> HeapMutexWithPeer2Exclusion_mutexMap;
    inline std::unordered_map<std::string, Lobby> HeapMutexWithPeer2Exclusion_map;
    inline std::string HeapMutexWithPeer2Exclusion_uuid;

    inline std::vector<std::thread> HeapMutexWithPeer2Exclusion_threads;
    inline std::atomic<int> HeapMutexWithPeer2Exclusion_counter;

    inline std::mutex HeapMutexWithPeer2Exclusion_conditionMutex;
    inline std::condition_variable HeapMutexWithPeer2Exclusion_wakeUpCondition;

    void UpdatePeer2(std::string uuid);
}  // namespace Tests

std::pair<std::string, bool> Tests::Peer2MutualExclusion() {
    int successFullIterations = 0;

    RLog("We will run the test %i times with %i threads\n\n", TEST_TIMES, NUM_THREADS);
    for (int i = 0; i < TEST_TIMES; i++) {
        bool isSuccess = false;
        std::string uuid = R::Utils::generateUUID(5);
        Lobby lobby;
        HeapMutexWithPeer2Exclusion_map[uuid] = lobby;
        HeapMutexWithPeer2Exclusion_mutexMap[uuid] = std::make_unique<std::mutex>();

        for (int i = 0; i < NUM_THREADS; i++) {
            HeapMutexWithPeer2Exclusion_threads.push_back(std::thread(UpdatePeer2, uuid));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        HeapMutexWithPeer2Exclusion_wakeUpCondition.notify_all();

        for (auto& thread : HeapMutexWithPeer2Exclusion_threads) {
            thread.join();
        }

        HeapMutexWithPeer2Exclusion_threads.erase(
            HeapMutexWithPeer2Exclusion_threads.begin(),
            HeapMutexWithPeer2Exclusion_threads.end()
        );

        if (HeapMutexWithPeer2Exclusion_counter == 1) {
            isSuccess = true;
            successFullIterations++;
        }

        std::cout << "[iteration-" << i + 1 << "] We updated peer2 " << HeapMutexWithPeer2Exclusion_counter << " times " << '\n';
        std::cout << "[iteration-" << i + 1 << "] Was a " << ((isSuccess) ? "success" : "failure") << "\n\n";

        HeapMutexWithPeer2Exclusion_counter = 0;
        isSuccess = false;
    }

    return std::pair<std::string, bool>("Tests::Peer2MutualExclusion", successFullIterations == TEST_TIMES);
}

void Tests::UpdatePeer2(std::string uuid) {
    Peer peer2;
    peer2.socket = R::Utils::randomNumber(1, 10);
    peer2.family = R::Utils::randomNumber(1, 50);
    peer2.ipAddress = {R::Utils::randomUintNumber(1, 1000)};

    std::unique_lock<std::mutex> lock(HeapMutexWithPeer2Exclusion_conditionMutex);
    HeapMutexWithPeer2Exclusion_wakeUpCondition.wait(lock);

    if (HeapMutexWithPeer2Exclusion_map[uuid].SetPeer2IfPossible(
            peer2, HeapMutexWithPeer2Exclusion_mutexMap[uuid].get()
        )) {
        HeapMutexWithPeer2Exclusion_counter++;
    }
}
