#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "board.h"
#include "search.h"
#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Voraxis {

class ThreadPool {
public:
    static void init(int threadCount = 1);
    static void setThreadCount(int count);
    static int getThreadCount();
    static Move startSearch(Board& rootBoard, const Search::SearchLimits& limits);
    static long long getTotalNodes();
    static int getMaxSeldepth();
    static void stop();
    static void cleanup();
};

}

#endif
