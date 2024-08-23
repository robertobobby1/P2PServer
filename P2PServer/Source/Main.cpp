#include "P2PServer.h"
#include <execinfo.h>

void handler(int sig) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// #define RUN_TESTS

#if defined(RUN_TESTS)
#    include "Tests.h"
#endif

int main() {
    signal(SIGSEGV, handler);
#if defined(RUN_TESTS)
#    include "Tests.h"
    Tests::run();
#else
    P2PServer::run();
#endif
    return 0;
}