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

// Search limits for time management and depth
struct SearchLimits {
    int time = -1;            // Allocated time in ms (-1 = no limit)
    int depth = -1;           // Max depth limit (-1 = unlimited/default)
    long long startTime = 0;  // Timestamp in ms when search started
};

// Transposition Table Flags
enum TTFlag : uint8_t {
    TT_NONE = 0,
    TT_EXACT,
    TT_LOWERBOUND,
    TT_UPPERBOUND
};

// Transposition Table Entry
struct TTEntry {
    uint64_t key = 0;
    int score = 0;
    int depth = 0;
    TTFlag flag = TT_NONE;
    Move bestMove;
};

// Constants for evaluation bounds & mate normalization
constexpr int MATE_SCORE = 1000000;
constexpr int MATE_THRESHOLD = 900000;
constexpr size_t TT_ENTRIES = 1048576; // 1,048,576 entries (~40MB)

// Global search state
extern std::atomic<bool> stopSearch;
extern size_t ttMask;
extern std::vector<TTEntry> ttTable;

// Time and limits utilities
long long nowMs();
void checkTime(const SearchLimits& limits);

// Transposition Table functions
int scoreToTT(int score, int ply);
int scoreFromTT(int score, int ply);
TTEntry* probeTT(uint64_t key);
void storeTT(uint64_t key, int depth, int score, TTFlag flag, Move bestMove, int ply);
void clearTT();
void setTTSize(size_t mbSize);
int getHashFull();
std::vector<Move> extractPV(Board board, int depth);

// Search Worker Context (Per-thread search instance)
struct SearchWorker {
    int threadId = 0;
    Board board;
    SearchLimits limits;
    long long searchedNodes = 0;
    int seldepth = 0;
    int historyTable[12][64];
    Move killerMoves[64][2];
    Move counterMoveTable[64][64];
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

// Move scoring and ordering
int getPieceValue(int pType);
bool isPseudoLegal(const Board& b, const Move& m);
bool isLegalMove(Board& b, const Move& m);

// Helper for standalone search
Move getBestMove(Board& board, const SearchLimits& limits = SearchLimits());

} // namespace Search
} // namespace Voraxis

#endif // SEARCH_H
