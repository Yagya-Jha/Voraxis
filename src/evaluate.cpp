#include "evaluate.h"

namespace Voraxis {
namespace Evaluate {

// Simple piece square tables from White's perspective
static const int pawnTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int knightTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const int bishopTable[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const int rookTable[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0
};

int evaluate(const Board& board) {
    int score = 0;

    // Material & Position scores for White
    uint64_t wPawns = board.pieceBB[W_PAWN];
    while (wPawns) {
        int sq = __builtin_ctzll(wPawns);
        wPawns &= wPawns - 1;
        score += PAWN_VALUE + pawnTable[sq ^ 56];
    }

    uint64_t wKnights = board.pieceBB[W_KNIGHT];
    while (wKnights) {
        int sq = __builtin_ctzll(wKnights);
        wKnights &= wKnights - 1;
        score += KNIGHT_VALUE + knightTable[sq ^ 56];
    }

    uint64_t wBishops = board.pieceBB[W_BISHOP];
    while (wBishops) {
        int sq = __builtin_ctzll(wBishops);
        wBishops &= wBishops - 1;
        score += BISHOP_VALUE + bishopTable[sq ^ 56];
    }

    uint64_t wRooks = board.pieceBB[W_ROOK];
    while (wRooks) {
        int sq = __builtin_ctzll(wRooks);
        wRooks &= wRooks - 1;
        score += ROOK_VALUE + rookTable[sq ^ 56];
    }

    uint64_t wQueens = board.pieceBB[W_QUEEN];
    while (wQueens) {
        int sq = __builtin_ctzll(wQueens);
        wQueens &= wQueens - 1;
        score += QUEEN_VALUE;
    }

    // Material & Position scores for Black
    uint64_t bPawns = board.pieceBB[B_PAWN];
    while (bPawns) {
        int sq = __builtin_ctzll(bPawns);
        bPawns &= bPawns - 1;
        score -= (PAWN_VALUE + pawnTable[sq]);
    }

    uint64_t bKnights = board.pieceBB[B_KNIGHT];
    while (bKnights) {
        int sq = __builtin_ctzll(bKnights);
        bKnights &= bKnights - 1;
        score -= (KNIGHT_VALUE + knightTable[sq]);
    }

    uint64_t bBishops = board.pieceBB[B_BISHOP];
    while (bBishops) {
        int sq = __builtin_ctzll(bBishops);
        bBishops &= bBishops - 1;
        score -= (BISHOP_VALUE + bishopTable[sq]);
    }

    uint64_t bRooks = board.pieceBB[B_ROOK];
    while (bRooks) {
        int sq = __builtin_ctzll(bRooks);
        bRooks &= bRooks - 1;
        score -= (ROOK_VALUE + rookTable[sq]);
    }

    uint64_t bQueens = board.pieceBB[B_QUEEN];
    while (bQueens) {
        int sq = __builtin_ctzll(bQueens);
        bQueens &= bQueens - 1;
        score -= QUEEN_VALUE;
    }

    return score;
}

} // namespace Evaluate
} // namespace Voraxis
