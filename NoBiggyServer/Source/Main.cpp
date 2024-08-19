#include "Server.h"

// #define RUN_TESTS

#if defined(RUN_TESTS)
#    include "Tests.h"
#endif

int main() {
#if defined(RUN_TESTS)
#    include "Tests.h"
    Tests::run();
#else
    Server::run();
#endif
    return 0;
}