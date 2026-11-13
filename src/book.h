#ifndef BOOK_H
#define BOOK_H

#include "board.h"
#include <string>
#include <vector>

namespace Voraxis {
namespace Book {

bool init(const std::string& bookPath = "books/komodo.bin");

uint64_t computePolyglotHash(const Board& board);

Move getBookMove(Board& board, int maxMoves = 30);

void close();

bool isLoaded();

}
}

#endif
