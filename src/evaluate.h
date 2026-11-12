#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

namespace Voraxis {
namespace Evaluate {

void init();

// Scores position from White's perspective (positive = White advantage)
int evaluate(const Board& board);

} // namespace Evaluate
} // namespace Voraxis

#endif // EVALUATE_H
