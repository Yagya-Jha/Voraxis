#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "movegen.h"
#include <chrono>
#include <vector>
#include <cstdint>

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

// Global search state and heuristics
extern bool stopSearch;
extern long long searchedNodes;
extern int historyTable[12][64];
extern Move killerMoves[64][2];
extern Move counterMoveTable[64][64];
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

// Move scoring and ordering
int getPieceValue(int pType);
int scoreMove(const Board& board, const Move move, int ply, const Move& ttMove, const Move& prevMove = Move());
int scoreMove(const Board& board, const Move move, int ply = 0);
std::vector<Move> returnSortedMoves(Board& board, int ply = 0, const Move& prevMove = Move());

// Search algorithms
Move getBestMove(Board& board, const SearchLimits& limits = SearchLimits());
int quiescence(Board& board, int alpha, int beta, const SearchLimits& limits, int ply = 0);
int negamax(Board& board, int depth, int alpha, int beta, const SearchLimits& limits, int ply = 0, bool allowNull = true, Move excludedMove = Move(), Move prevMove = Move());

} // namespace Search
} // namespace Voraxis

#endif // SEARCH_H
