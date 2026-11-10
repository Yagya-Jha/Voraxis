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

// Initialize Syzygy tablebases from a directory path (semicolon-separated for multiple paths on Windows)
bool init(const std::string& path);

// Free any allocated tablebase resources
void free();

// Check if tablebases are active
bool isLoaded();

// Get the maximum piece count supported by loaded tablebases (e.g., 5)
int getMaxPieces();

// Probe Win/Draw/Loss during search
// Returns WDLResult (WDL_FAILED if not in TB, castling possible, or probe error)
WDLResult probeWDL(const Board& board);

// Probe Distance-To-Zero at root
// Returns true if a tablebase move was found, populating bestMove and wdlScore.
bool probeRoot(const Board& board, Move& bestMove, int& wdlScore);

} // namespace Syzygy
} // namespace Voraxis

#endif // SYZYGY_BRIDGE_H
