#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "movegen.h"
#include <chrono>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>

namespace Voraxis {
namespace Search {

struct SearchLimits {
    int time = -1;
    int optimumTime = -1;
    int depth = -1;
    long long startTime = 0;
};

enum TTFlag : uint8_t {
    TT_NONE = 0,
    TT_EXACT,
    TT_LOWERBOUND,
    TT_UPPERBOUND
};

struct TTEntry {
    uint64_t key = 0;
    int16_t score = 0;
    int16_t staticEval = 0;
    int8_t depth = 0;
    uint8_t flag = TT_NONE;
    uint8_t gen = 0;
    Move bestMove;
};

constexpr int MATE_SCORE = 1000000;
constexpr int MATE_THRESHOLD = 900000;
constexpr size_t TT_ENTRIES = 1048576;

extern std::atomic<bool> stopSearch;
extern size_t ttMask;
extern std::vector<TTEntry> ttTable;
extern uint8_t ttGen;

long long nowMs();
void checkTime(const SearchLimits& limits, long long nodes);

int scoreToTT(int score, int ply);
int scoreFromTT(int score, int ply);
TTEntry* probeTT(uint64_t key);
void storeTT(uint64_t key, int depth, int score, TTFlag flag, Move bestMove, int ply, int staticEval = 0);
void clearTT();
void setTTSize(size_t mbSize);
int getHashFull();
std::vector<Move> extractPV(Board board, int depth);

struct SearchWorker {
    int threadId = 0;
    Board board;
    SearchLimits limits;
    long long searchedNodes = 0;
    int seldepth = 0;
    int historyTable[12][64];
    int conthist[12][64][12][64];
    Move killerMoves[64][2];
    Move counterMoveTable[64][64];
    int staticEvalStack[128];
    Move bestMove;
    int bestScore = 0;

    SearchWorker(int id = 0);
    void reset();

    Move searchRoot(bool isMainThread);
    int negamax(int depth, int alpha, int beta, int ply, bool allowNull, Move excludedMove, Move prevMove);
    int quiescence(int alpha, int beta, int ply);
    int scoreMove(const Move& move, int ply, const Move& ttMove, const Move& prevMove);
    std::vector<Move> returnSortedMoves(int ply, const Move& prevMove);
};

int getPieceValue(int pType);
bool isPseudoLegal(const Board& b, const Move& m);
bool isLegalMove(Board& b, const Move& m);

Move getBestMove(Board& board, const SearchLimits& limits = SearchLimits());

}
}

#endif
