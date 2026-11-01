#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

namespace Voraxis {
namespace Evaluate {

// Piece values in centipawns
constexpr int PAWN_VALUE = 100;
constexpr int KNIGHT_VALUE = 320;
constexpr int BISHOP_VALUE = 330;
constexpr int ROOK_VALUE = 500;
constexpr int QUEEN_VALUE = 900;
constexpr int KING_VALUE = 20000;

// Scores position from White's perspective (positive = White advantage)
int evaluate(const Board& board);

} // namespace Evaluate
} // namespace Voraxis

#endif // EVALUATE_H
