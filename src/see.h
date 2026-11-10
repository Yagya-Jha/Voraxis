#ifndef SEE_H
#define SEE_H

#include "board.h"
#include <cstdint>

namespace Voraxis {
namespace SEE {

// Returns the Static Exchange Evaluation score of a move in centipawns.
// Positive: winning capture.
// Zero: equal trade.
// Negative: losing capture / sacrifice.
int evaluate(const Board& board, const Move& move);

// Fast check: returns true if SEE(move) >= threshold (default 0)
bool hasPositiveSEE(const Board& board, const Move& move, int threshold = 0);

} // namespace SEE
} // namespace Voraxis

#endif // SEE_H
