#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"
#include <algorithm>

namespace Voraxis {
namespace Evaluate {

// Opening / Middlegame and Endgame piece values in centipawns
constexpr int MG_PAWN = 100, EG_PAWN = 130;
constexpr int MG_KNIGHT = 320, EG_KNIGHT = 310;
constexpr int MG_BISHOP = 330, EG_BISHOP = 340;
constexpr int MG_ROOK = 500, EG_ROOK = 540;
constexpr int MG_QUEEN = 900, EG_QUEEN = 930;

// Maximum phase weight (4N*1 + 4B*1 + 4R*2 + 2Q*4 = 24)
constexpr int MAX_PHASE = 24;

// Continuous game phase calculation (24 = pure MG, 0 = pure EG)
int getGamePhase(const Board& board);

// Scores position from White's perspective with tapered evaluation
int evaluate(const Board& board);

} // namespace Evaluate
} // namespace Voraxis

#endif // EVALUATE_H
