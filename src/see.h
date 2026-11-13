#ifndef SEE_H
#define SEE_H

#include "board.h"
#include <cstdint>

namespace Voraxis {
namespace SEE {

int evaluate(const Board& board, const Move& move);

bool hasPositiveSEE(const Board& board, const Move& move, int threshold = 0);

}
}

#endif
