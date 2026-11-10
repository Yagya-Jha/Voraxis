#ifndef UCI_H
#define UCI_H

#include "board.h"
#include <string>

namespace Voraxis {
namespace UCI {

struct UCIOptions {
    int hashMB = 64;
    int threads = 1;
    int moveOverhead = 10;
    bool ownBook = true;
    std::string bookPath = "books/komodo.bin";
    std::string syzygyPath = "books/syzygy";
};

extern UCIOptions options;

void loop();
void handleCommand(const std::string& line, Board& board, bool& running);

} // namespace UCI
} // namespace Voraxis

#endif // UCI_H
