#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "movegen.h"

namespace Voraxis {
namespace Search {

// Chooses a move. Consumes Movegen results (first legal move for now).
Move getBestMove(Board& board);

// Minimax search algorithm
//int minimax(Board& board, int depth, bool isMaximizing);

int quiescence(Board& board, int alpha, int beta);
int scoreMove(const Move move);
int getPieceValue(int pType);
std::vector<Move> returnSortedMoves(Board& board);
//negamax search algorithm
int negamax(Board& board, int depth, int alpha, int beta);
} // namespace Search
} // namespace Voraxis

#endif // SEARCH_H
