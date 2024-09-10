#include "P2PServer.h"

// #define RUN_TESTS

#if defined(RUN_TESTS)
#    include "Tests.h"
#endif

int main() {
#if defined(RUN_TESTS)
#    include "Tests.h"
    Tests::run();
#else
    P2PServer::run();
#endif
    return 0;
}