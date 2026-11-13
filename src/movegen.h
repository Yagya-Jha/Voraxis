#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"
#include <vector>

namespace Voraxis {
namespace Movegen {

void initAttackTables();

uint64_t getPawnAttacks(int color, int sq);
uint64_t getKnightAttacks(int sq);
uint64_t getKingAttacks(int sq);
uint64_t getBishopAttacks(int sq, uint64_t occ);
uint64_t getRookAttacks(int sq, uint64_t occ);
uint64_t getQueenAttacks(int sq, uint64_t occ);

bool isSquareAttacked(const Board& board, int sq, int attackerColor);
bool inCheck(const Board& board, int color);

enum MoveGenType {
    GEN_ALL = 0,
    GEN_CAPTURES = 1,
    GEN_QUIETS = 2
};

std::vector<Move> generateLegalMoves(Board& board, MoveGenType type = GEN_ALL);

bool isCheckmate(Board& board);
bool isStalemate(Board& board);
bool isDraw(Board& board);

uint64_t perft(Board& board, int depth);
bool runPerftSuite();

}
}

#endif
