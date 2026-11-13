#include "threadpool.h"
#include <iostream>
#include <algorithm>

namespace Voraxis {

struct ThreadHandle {
    int id;
#ifdef _WIN32
    HANDLE hThread = NULL;
    HANDLE hStartEvent = NULL;
    HANDLE hDoneEvent = NULL;
#endif
    std::atomic<bool> exitRequested{false};
};

static std::vector<std::unique_ptr<Search::SearchWorker>> workers;
static std::vector<std::unique_ptr<ThreadHandle>> handles;
static int activeThreadCount = 1;
static bool poolInitialized = false;

#ifdef _WIN32
static DWORD WINAPI workerThreadProc(LPVOID lpParam) {
    ThreadHandle* th = static_cast<ThreadHandle*>(lpParam);
    while (!th->exitRequested.load()) {
        WaitForSingleObject(th->hStartEvent, INFINITE);
        if (th->exitRequested.load()) break;

        workers[th->id]->searchRoot(false);
        SetEvent(th->hDoneEvent);
    }
    return 0;
}
#endif

void ThreadPool::init(int count) {
    if (poolInitialized) {
        setThreadCount(count);
        return;
    }
    count = std::max(1, std::min(128, count));
    activeThreadCount = count;
    workers.resize(count);
    handles.resize(count);

    workers[0] = std::make_unique<Search::SearchWorker>(0);
    handles[0] = nullptr;

#ifdef _WIN32
    for (int i = 1; i < count; ++i) {
        workers[i] = std::make_unique<Search::SearchWorker>(i);
        auto th = std::make_unique<ThreadHandle>();
        th->id = i;
        th->hStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        th->hDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        th->exitRequested.store(false);
        th->hThread = CreateThread(NULL, 0, workerThreadProc, th.get(), 0, NULL);
        handles[i] = std::move(th);
    }
#endif
    poolInitialized = true;
}

void ThreadPool::setThreadCount(int count) {
    count = std::max(1, std::min(128, count));
    if (count == activeThreadCount && poolInitialized) return;
    cleanup();
    init(count);
}

int ThreadPool::getThreadCount() {
    return activeThreadCount;
}

void ThreadPool::cleanup() {
    if (!poolInitialized) return;
    Search::stopSearch.store(true);
#ifdef _WIN32
    for (size_t i = 1; i < handles.size(); ++i) {
        if (handles[i]) {
            handles[i]->exitRequested.store(true);
            SetEvent(handles[i]->hStartEvent);
            WaitForSingleObject(handles[i]->hThread, 2000);
            CloseHandle(handles[i]->hThread);
            CloseHandle(handles[i]->hStartEvent);
            CloseHandle(handles[i]->hDoneEvent);
            handles[i] = nullptr;
        }
    }
#endif
    workers.clear();
    handles.clear();
    activeThreadCount = 1;
    poolInitialized = false;
}

void ThreadPool::stop() {
    Search::stopSearch.store(true);
}

Move ThreadPool::startSearch(Board& rootBoard, const Search::SearchLimits& limits) {
    if (!poolInitialized) init(1);

    Search::stopSearch.store(false);

    workers[0]->reset();
    workers[0]->board = rootBoard;
    workers[0]->limits = limits;

#ifdef _WIN32

    for (int i = 1; i < activeThreadCount; ++i) {
        workers[i]->reset();
        workers[i]->board = rootBoard;
        workers[i]->limits = limits;
        ResetEvent(handles[i]->hDoneEvent);
        SetEvent(handles[i]->hStartEvent);
    }
#endif

    Move bestMove = workers[0]->searchRoot(true);

    Search::stopSearch.store(true);
#ifdef _WIN32
    for (int i = 1; i < activeThreadCount; ++i) {
        WaitForSingleObject(handles[i]->hDoneEvent, INFINITE);
    }
#endif

    return bestMove;
}

long long ThreadPool::getTotalNodes() {
    long long total = 0;
    for (int i = 0; i < activeThreadCount; ++i) {
        if (workers[i]) total += workers[i]->searchedNodes;
    }
    return total;
}

int ThreadPool::getMaxSeldepth() {
    int maxSel = 0;
    for (int i = 0; i < activeThreadCount; ++i) {
        if (workers[i]) maxSel = std::max(maxSel, workers[i]->seldepth);
    }
    return maxSel;
}

}
