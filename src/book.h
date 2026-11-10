#ifndef BOOK_H
#define BOOK_H

#include "board.h"
#include <string>
#include <vector>

namespace Voraxis {
namespace Book {

// Initialize and load the opening book (komodo.bin)
bool init(const std::string& bookPath = "books/komodo.bin");

// Compute 64-bit PolyGlot Zobrist hash for a board position
uint64_t computePolyglotHash(const Board& board);

// Lookup a book move for the board (weighted selection among candidate book moves)
// Returns Move() if out of book or if book is not loaded
Move getBookMove(Board& board, int maxMoves = 30);

// Close and free book resources
void close();

// Check if book is currently loaded
bool isLoaded();

} // namespace Book
} // namespace Voraxis

#endif // BOOK_H
