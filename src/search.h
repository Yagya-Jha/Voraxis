#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "movegen.h"
#include <chrono>

namespace Voraxis {
namespace Search {

struct SearchLimits {
    int time = -1;            // Allocated time in ms (-1 = no limit)
    int depth = -1;           // Max depth limit (-1 = unlimited/default)
    long long startTime = 0;  // Timestamp in ms when search started
};

extern bool stopSearch;
extern long long searchedNodes;

long long nowMs();
void checkTime(const SearchLimits& limits);

// Chooses a move. Consumes Movegen results.
Move getBestMove(Board& board, const SearchLimits& limits = SearchLimits());

int quiescence(Board& board, int alpha, int beta, const SearchLimits& limits);
int scoreMove(const Move move);
int getPieceValue(int pType);
std::vector<Move> returnSortedMoves(Board& board);

// Negamax search algorithm
int negamax(Board& board, int depth, int alpha, int beta, const SearchLimits& limits);

} // namespace Search
} // namespace Voraxis

#endif // SEARCH_H
