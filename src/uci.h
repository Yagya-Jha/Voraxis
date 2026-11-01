#ifndef UCI_H
#define UCI_H

#include "board.h"
#include <string>

namespace Voraxis {
namespace UCI {

void loop();
void handleCommand(const std::string& line, Board& board, bool& running);

} // namespace UCI
} // namespace Voraxis

#endif // UCI_H
