#ifndef SYZYGY_BRIDGE_H
#define SYZYGY_BRIDGE_H

#include <string>
#include <cstdint>
#include "board.h"

namespace Voraxis {
namespace Syzygy {

enum WDLResult {
    WDL_FAILED = -1,
    WDL_LOSS = 0,
    WDL_BLESSED_LOSS = 1,
    WDL_DRAW = 2,
    WDL_CURSED_WIN = 3,
    WDL_WIN = 4
};

bool init(const std::string& path);

void free();

bool isLoaded();

int getMaxPieces();

WDLResult probeWDL(const Board& board);

bool probeRoot(const Board& board, Move& bestMove, int& wdlScore);

}
}

#endif
