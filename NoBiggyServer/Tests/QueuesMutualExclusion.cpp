#include <unordered_map>
#include <mutex>
#include <thread>

#include "Tests.h"
#include "NetworkStructs.h"
#include "Common.h"

namespace Tests {
    inline const int NUM_THREADS = 100;
    inline const int TEST_TIMES = 5;

    inline std::queue<std::string> QueuesMutualExclusion_queue;
    inline std::mutex QueuesMutualExclusion_queueMutex;

    inline std::vector<std::thread> QueuesMutualExclusion_threads;
    inline std::atomic<int> QueuesMutualExclusion_setCounter;
    inline std::atomic<int> QueuesMutualExclusion_getCounter;
    inline std::atomic<int> QueuesMutualExclusion_getOnEmptyQueue;

    inline std::mutex QueuesMutualExclusion_conditionMutex;
    inline std::condition_variable QueuesMutualExclusion_wakeUpCondition;

    void RandomGetOrSetQueue();

}  // namespace Tests

std::pair<std::string, bool> Tests::QueuesMutualExclusion() {
    QueuesMutualExclusion_queue = std::queue<std::string>();
    int successFullIterations = 0;
    bool isSuccess = false;

    printf("We will run the test %i times with %i threads\n\n", TEST_TIMES, NUM_THREADS);
    for (int i = 0; i < TEST_TIMES; i++) {
        for (int i = 0; i < NUM_THREADS; i++) {
            QueuesMutualExclusion_threads.push_back(std::thread(RandomGetOrSetQueue));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        QueuesMutualExclusion_wakeUpCondition.notify_all();

        for (auto& thread : QueuesMutualExclusion_threads) {
            thread.join();
        }

        QueuesMutualExclusion_threads.erase(
            QueuesMutualExclusion_threads.begin(),
            QueuesMutualExclusion_threads.end()
        );

        int succesfulGets = QueuesMutualExclusion_getCounter - QueuesMutualExclusion_getOnEmptyQueue;
        if ((QueuesMutualExclusion_setCounter - succesfulGets) == QueuesMutualExclusion_queue.size()) {
            isSuccess = true;
            successFullIterations++;
        }

        std::cout << "The get counter is " << QueuesMutualExclusion_getCounter << "\n";
        std::cout << "The set counter is " << QueuesMutualExclusion_setCounter << "\n";
        std::cout << "The empty queue counter is " << QueuesMutualExclusion_getOnEmptyQueue << "\n";
        std::cout << "The queue's length is " << QueuesMutualExclusion_queue.size() << "\n";
        printf("[iteration-%i] The test was a %s\n\n", i + 1, (isSuccess ? "success" : "failure"));

        QueuesMutualExclusion_getCounter = 0;
        QueuesMutualExclusion_setCounter = 0;
        QueuesMutualExclusion_getOnEmptyQueue = 0;
        isSuccess = false;

        std::queue<std::string> emptyQueue;
        std::swap(QueuesMutualExclusion_queue, emptyQueue);
    }

    return std::pair<std::string, bool>("Tests::QueuesMutualExclusion", successFullIterations == TEST_TIMES);
}

void Tests::RandomGetOrSetQueue() {
    if (Common::randomNumber(1, 10) % 2 == 0) {
        Common::setThreadSafeToQueue(QueuesMutualExclusion_queue, QueuesMutualExclusion_queueMutex, Common::generateUUID(2));
        QueuesMutualExclusion_setCounter++;
    } else {
        if (Common::getThreadSafeFromQueue(QueuesMutualExclusion_queue, QueuesMutualExclusion_queueMutex) == "") {
            QueuesMutualExclusion_getOnEmptyQueue++;
        }

        QueuesMutualExclusion_getCounter++;
    }
}
