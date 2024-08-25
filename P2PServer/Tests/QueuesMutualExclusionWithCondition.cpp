#include <unordered_map>
#include <mutex>
#include <thread>

#include "Tests.h"
#include "NetworkStructs.h"
#include "R.h"

namespace Tests {
    inline const int NUM_THREADS = 2000;
    inline const int TEST_TIMES = 5;

    inline std::queue<std::string> QueuesMutualExclusionWithCondition_queue;
    inline std::mutex QueuesMutualExclusionWithCondition_queueMutex;
    inline std::condition_variable QueuesMutualExclusionWithCondition_queueCondition;
    inline std::mutex QueuesMutualExclusionWithCondition_queueConditionMutex;

    inline std::atomic<int> QueuesMutualExclusionWithCondition_emptyQueueNotifier = 0;

    inline std::vector<std::thread> QueuesMutualExclusionWithCondition_threads;
    inline std::atomic<int> QueuesMutualExclusionWithCondition_setCounter;
    inline std::atomic<int> QueuesMutualExclusionWithCondition_getCounter;
    inline std::atomic<int> QueuesMutualExclusionWithCondition_waitingCounter;

    inline std::mutex QueuesMutualExclusionWithCondition_conditionMutex;
    inline std::condition_variable QueuesMutualExclusionWithCondition_wakeUpCondition;

    void RandomGetOrSetQueueWithCondition();

}  // namespace Tests

std::pair<std::string, bool> Tests::QueuesMutualExclusionWithCondition() {
    QueuesMutualExclusionWithCondition_queue = std::queue<std::string>();
    int successFullIterations = 0;
    bool isSuccess = false;

    RLog("We will run the test %i times with %i threads\n\n", TEST_TIMES, NUM_THREADS);
    for (int i = 0; i < TEST_TIMES; i++) {
        for (int i = 0; i < NUM_THREADS; i++) {
            QueuesMutualExclusionWithCondition_threads.push_back(std::thread(RandomGetOrSetQueueWithCondition));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        QueuesMutualExclusionWithCondition_wakeUpCondition.notify_all();

        std::this_thread::sleep_for(std::chrono::seconds(5));
        QueuesMutualExclusionWithCondition_queueCondition.notify_all();

        for (auto& thread : QueuesMutualExclusionWithCondition_threads) {
            thread.join();
        }

        QueuesMutualExclusionWithCondition_threads.erase(
            QueuesMutualExclusionWithCondition_threads.begin(),
            QueuesMutualExclusionWithCondition_threads.end()
        );

        if ((QueuesMutualExclusionWithCondition_setCounter - QueuesMutualExclusionWithCondition_getCounter) == QueuesMutualExclusionWithCondition_queue.size() &&
            !(QueuesMutualExclusionWithCondition_waitingCounter > 0 && QueuesMutualExclusionWithCondition_queue.size() > 0)) {
            isSuccess = true;
            successFullIterations++;
        }

        std::cout << "[iteration-" << i + 1 << "] The get counter is " << QueuesMutualExclusionWithCondition_getCounter << "\n";
        std::cout << "[iteration-" << i + 1 << "] The set counter is " << QueuesMutualExclusionWithCondition_setCounter << "\n";
        std::cout << "[iteration-" << i + 1 << "] The threads that waited due to the queue not having enough data is " << QueuesMutualExclusionWithCondition_waitingCounter << "\n";
        std::cout << "[iteration-" << i + 1 << "] The queue's length is " << QueuesMutualExclusionWithCondition_queue.size() << "\n";
        std::cout << "[iteration-" << i + 1 << "] The test was a " << (isSuccess ? "success" : "failure") << "\n\n";

        QueuesMutualExclusionWithCondition_getCounter = 0;
        QueuesMutualExclusionWithCondition_setCounter = 0;
        QueuesMutualExclusionWithCondition_waitingCounter = 0;
        isSuccess = false;

        std::queue<std::string> emptyQueue;
        std::swap(QueuesMutualExclusionWithCondition_queue, emptyQueue);
    }

    return std::pair<std::string, bool>("Tests::QueuesMutualExclusionWithCondition", successFullIterations == TEST_TIMES);
}

void Tests::RandomGetOrSetQueueWithCondition() {
    if (R::Utils::randomNumber(1, 10) % 2 == 0) {
        R::Utils::setThreadSafeToQueue(QueuesMutualExclusionWithCondition_queue, QueuesMutualExclusionWithCondition_queueMutex, R::Utils::generateUUID(2));
        QueuesMutualExclusionWithCondition_setCounter++;
        {
            std::unique_lock<std::mutex> lock(QueuesMutualExclusionWithCondition_queueConditionMutex);
            QueuesMutualExclusionWithCondition_queueCondition.notify_one();
        }
    } else {
        QueuesMutualExclusionWithCondition_waitingCounter++;
        if (R::Utils::getThreadSafeFromQueue(QueuesMutualExclusionWithCondition_queue, QueuesMutualExclusionWithCondition_queueMutex, QueuesMutualExclusionWithCondition_queueCondition) != "") {
            QueuesMutualExclusionWithCondition_getCounter++;
            QueuesMutualExclusionWithCondition_waitingCounter--;
        }
    }
}
