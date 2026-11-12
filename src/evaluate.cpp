#include "evaluate.h"
#include "movegen.h"
#include <algorithm>
#include <cstdint>

namespace Voraxis {
namespace Evaluate {

// ============================================================================
// 13.1 PeSTO's Piece Values & Piece-Square Tables (Tomasz Michniewski)
// ============================================================================

constexpr int MG_VAL[6] = { 82, 337, 365, 477, 1025, 0 };
constexpr int EG_VAL[6] = { 94, 281, 297, 512,  936, 0 };

// Tables defined from White's perspective with A8 = index 0, A1 = index 56.
// White square sq -> index = sq ^ 56
// Black square sq -> index = sq

static const int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static const int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static const int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

static const int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

static const int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

static const int eg_bishop_table[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};

static const int mg_rook_table[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};

static const int eg_rook_table[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20,
};

static const int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -48, -18,
};

static const int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  18,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

static const int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

static const int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

// ============================================================================
// Bitboard Masks for Fast Evaluation
// ============================================================================

static uint64_t fileMask[8];
static uint64_t rankMask[8];
static uint64_t passedPawnMask[2][64];
static uint64_t isolatedPawnMask[8];
static bool evalInitialized = false;

// Passed pawn rank bonuses (index = relative rank 0..7)
static const int mgPassedBonus[8] = { 0, 5, 10, 20, 35, 60, 100, 0 };
static const int egPassedBonus[8] = { 0, 10, 20, 35, 60, 100, 160, 0 };

void init() {
    if (evalInitialized) return;

    for (int f = 0; f < 8; ++f) {
        fileMask[f] = 0x0101010101010101ULL << f;
        isolatedPawnMask[f] = (f > 0 ? (0x0101010101010101ULL << (f - 1)) : 0ULL) |
                              (f < 7 ? (0x0101010101010101ULL << (f + 1)) : 0ULL);
    }

    for (int r = 0; r < 8; ++r) {
        rankMask[r] = 0xFFULL << (r * 8);
    }

    for (int sq = 0; sq < 64; ++sq) {
        int f = squareFile(sq);
        int r = squareRank(sq);

        uint64_t files = (f > 0 ? fileMask[f - 1] : 0ULL) | fileMask[f] | (f < 7 ? fileMask[f + 1] : 0ULL);

        uint64_t whiteFrontRanks = 0ULL;
        for (int nextR = r + 1; nextR < 8; ++nextR) {
            whiteFrontRanks |= rankMask[nextR];
        }
        passedPawnMask[WHITE][sq] = files & whiteFrontRanks;

        uint64_t blackFrontRanks = 0ULL;
        for (int prevR = r - 1; prevR >= 0; --prevR) {
            blackFrontRanks |= rankMask[prevR];
        }
        passedPawnMask[BLACK][sq] = files & blackFrontRanks;
    }

    evalInitialized = true;
}

// Inline fast pawn attacks
static inline uint64_t whitePawnAttacks(uint64_t pawns) {
    return ((pawns << 7) & ~0x8080808080808080ULL) | ((pawns << 9) & ~0x0101010101010101ULL);
}

static inline uint64_t blackPawnAttacks(uint64_t pawns) {
    return ((pawns >> 7) & ~0x0101010101010101ULL) | ((pawns >> 9) & ~0x8080808080808080ULL);
}

// ============================================================================
// Full Evaluation Function
// ============================================================================

int evaluate(const Board& board) {
    if (!evalInitialized) init();

    int mgScore = 0;
    int egScore = 0;
    int gamePhase = 0;

    const uint64_t wPawns = board.pieceBB[W_PAWN];
    const uint64_t bPawns = board.pieceBB[B_PAWN];
    const uint64_t wKnights = board.pieceBB[W_KNIGHT];
    const uint64_t bKnights = board.pieceBB[B_KNIGHT];
    const uint64_t wBishops = board.pieceBB[W_BISHOP];
    const uint64_t bBishops = board.pieceBB[B_BISHOP];
    const uint64_t wRooks = board.pieceBB[W_ROOK];
    const uint64_t bRooks = board.pieceBB[B_ROOK];
    const uint64_t wQueens = board.pieceBB[W_QUEEN];
    const uint64_t bQueens = board.pieceBB[B_QUEEN];
    const uint64_t occ = board.occBB[BOTH];

    // Mobility safe zones (squares not attacked by enemy pawns)
    const uint64_t wPawnAttacks = whitePawnAttacks(wPawns);
    const uint64_t bPawnAttacks = blackPawnAttacks(bPawns);
    const uint64_t wMobilityArea = ~bPawnAttacks & ~board.occBB[WHITE];
    const uint64_t bMobilityArea = ~wPawnAttacks & ~board.occBB[BLACK];

    // ------------------------------------------------------------------------
    // 1. Material, PeSTO PSTs, & Phase Calculation
    // ------------------------------------------------------------------------

    // Pawns
    uint64_t bb = wPawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq ^ 56;
        mgScore += MG_VAL[PAWN] + mg_pawn_table[idx];
        egScore += EG_VAL[PAWN] + eg_pawn_table[idx];

        // 13.2 Passed pawns
        if ((passedPawnMask[WHITE][sq] & bPawns) == 0) {
            int relRank = squareRank(sq);
            mgScore += mgPassedBonus[relRank];
            egScore += egPassedBonus[relRank];
            // Protected passed pawn bonus
            if (Movegen::getPawnAttacks(WHITE, sq) & wPawns) {
                mgScore += 15;
                egScore += 25;
            }
        }
        // Isolated pawns
        int file = squareFile(sq);
        if ((isolatedPawnMask[file] & wPawns) == 0) {
            mgScore -= 12;
            egScore -= 18;
        }
        // Doubled pawns
        if ((passedPawnMask[WHITE][sq] & fileMask[file] & wPawns) != 0) {
            mgScore -= 10;
            egScore -= 15;
        }
    }

    bb = bPawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq;
        mgScore -= (MG_VAL[PAWN] + mg_pawn_table[idx]);
        egScore -= (EG_VAL[PAWN] + eg_pawn_table[idx]);

        // 13.2 Passed pawns
        if ((passedPawnMask[BLACK][sq] & wPawns) == 0) {
            int relRank = 7 - squareRank(sq);
            mgScore -= mgPassedBonus[relRank];
            egScore -= egPassedBonus[relRank];
            // Protected passed pawn bonus
            if (Movegen::getPawnAttacks(BLACK, sq) & bPawns) {
                mgScore -= 15;
                egScore -= 25;
            }
        }
        // Isolated pawns
        int file = squareFile(sq);
        if ((isolatedPawnMask[file] & bPawns) == 0) {
            mgScore += 12;
            egScore += 18;
        }
        // Doubled pawns
        if ((passedPawnMask[BLACK][sq] & fileMask[file] & bPawns) != 0) {
            mgScore += 10;
            egScore += 15;
        }
    }

    // Knights
    bb = wKnights;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq ^ 56;
        mgScore += MG_VAL[KNIGHT] + mg_knight_table[idx];
        egScore += EG_VAL[KNIGHT] + eg_knight_table[idx];
        gamePhase += 1;

        // Mobility
        uint64_t attacks = Movegen::getKnightAttacks(sq) & wMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore += (mob - 4) * 4;
        egScore += (mob - 4) * 4;

        // Outpost bonus (ranks 4..6, defended by friendly pawn, cannot be attacked by enemy pawn)
        int r = squareRank(sq);
        if (r >= 3 && r <= 5) {
            if ((Movegen::getPawnAttacks(BLACK, sq) & wPawns) &&
                !(passedPawnMask[WHITE][sq] & isolatedPawnMask[squareFile(sq)] & bPawns)) {
                mgScore += 20;
                egScore += 15;
            }
        }
    }

    bb = bKnights;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq;
        mgScore -= (MG_VAL[KNIGHT] + mg_knight_table[idx]);
        egScore -= (EG_VAL[KNIGHT] + eg_knight_table[idx]);
        gamePhase += 1;

        // Mobility
        uint64_t attacks = Movegen::getKnightAttacks(sq) & bMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore -= (mob - 4) * 4;
        egScore -= (mob - 4) * 4;

        // Outpost bonus
        int r = squareRank(sq);
        if (r >= 2 && r <= 4) {
            if ((Movegen::getPawnAttacks(WHITE, sq) & bPawns) &&
                !(passedPawnMask[BLACK][sq] & isolatedPawnMask[squareFile(sq)] & wPawns)) {
                mgScore -= 20;
                egScore -= 15;
            }
        }
    }

    // Bishops
    bb = wBishops;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq ^ 56;
        mgScore += MG_VAL[BISHOP] + mg_bishop_table[idx];
        egScore += EG_VAL[BISHOP] + eg_bishop_table[idx];
        gamePhase += 1;

        // Magic mobility
        uint64_t attacks = Movegen::getBishopAttacks(sq, occ) & wMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore += (mob - 7) * 3;
        egScore += (mob - 7) * 3;
    }

    bb = bBishops;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq;
        mgScore -= (MG_VAL[BISHOP] + mg_bishop_table[idx]);
        egScore -= (EG_VAL[BISHOP] + eg_bishop_table[idx]);
        gamePhase += 1;

        // Magic mobility
        uint64_t attacks = Movegen::getBishopAttacks(sq, occ) & bMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore -= (mob - 7) * 3;
        egScore -= (mob - 7) * 3;
    }

    // 13.3 Bishop Pair
    if (__builtin_popcountll(wBishops) >= 2) {
        mgScore += 30;
        egScore += 40;
    }
    if (__builtin_popcountll(bBishops) >= 2) {
        mgScore -= 30;
        egScore -= 40;
    }

    // Rooks
    bb = wRooks;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq ^ 56;
        mgScore += MG_VAL[ROOK] + mg_rook_table[idx];
        egScore += EG_VAL[ROOK] + eg_rook_table[idx];
        gamePhase += 2;

        // Magic mobility
        uint64_t attacks = Movegen::getRookAttacks(sq, occ) & wMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore += (mob - 7) * 2;
        egScore += (mob - 7) * 3;

        // Open & Semi-open files
        int f = squareFile(sq);
        if ((fileMask[f] & wPawns) == 0) {
            if ((fileMask[f] & bPawns) == 0) {
                mgScore += 20;
                egScore += 25;
            } else {
                mgScore += 10;
                egScore += 15;
            }
        }
        // 7th rank bonus
        if (squareRank(sq) == 6 && (bPawns & rankMask[6])) {
            mgScore += 25;
            egScore += 35;
        }
    }

    bb = bRooks;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq;
        mgScore -= (MG_VAL[ROOK] + mg_rook_table[idx]);
        egScore -= (EG_VAL[ROOK] + eg_rook_table[idx]);
        gamePhase += 2;

        // Magic mobility
        uint64_t attacks = Movegen::getRookAttacks(sq, occ) & bMobilityArea;
        int mob = __builtin_popcountll(attacks);
        mgScore -= (mob - 7) * 2;
        egScore -= (mob - 7) * 3;

        // Open & Semi-open files
        int f = squareFile(sq);
        if ((fileMask[f] & bPawns) == 0) {
            if ((fileMask[f] & wPawns) == 0) {
                mgScore -= 20;
                egScore -= 25;
            } else {
                mgScore -= 10;
                egScore -= 15;
            }
        }
        // 2nd rank bonus
        if (squareRank(sq) == 1 && (wPawns & rankMask[1])) {
            mgScore -= 25;
            egScore -= 35;
        }
    }

    // Queens
    bb = wQueens;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq ^ 56;
        mgScore += MG_VAL[QUEEN] + mg_queen_table[idx];
        egScore += EG_VAL[QUEEN] + eg_queen_table[idx];
        gamePhase += 4;
    }

    bb = bQueens;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        int idx = sq;
        mgScore -= (MG_VAL[QUEEN] + mg_queen_table[idx]);
        egScore -= (EG_VAL[QUEEN] + eg_queen_table[idx]);
        gamePhase += 4;
    }

    // Kings
    int wKingSq = board.getKingSquare(WHITE);
    int bKingSq = board.getKingSquare(BLACK);
    if (wKingSq != SQ_NONE) {
        mgScore += mg_king_table[wKingSq ^ 56];
        egScore += eg_king_table[wKingSq ^ 56];
    }
    if (bKingSq != SQ_NONE) {
        mgScore -= mg_king_table[bKingSq];
        egScore -= eg_king_table[bKingSq];
    }

    // ------------------------------------------------------------------------
    // 13.4 Lightweight King Safety (Middlegame only)
    // ------------------------------------------------------------------------
    if (wKingSq != SQ_NONE) {
        int f = squareFile(wKingSq);
        // Kingside pawn shield
        if (f >= 5) {
            int shield = __builtin_popcountll(wPawns & (fileMask[5] | fileMask[6] | fileMask[7]) & (rankMask[1] | rankMask[2]));
            mgScore += (shield - 3) * 15;
        } else if (f <= 2) { // Queenside pawn shield
            int shield = __builtin_popcountll(wPawns & (fileMask[0] | fileMask[1] | fileMask[2]) & (rankMask[1] | rankMask[2]));
            mgScore += (shield - 3) * 15;
        }
        // Open file on king
        if ((fileMask[f] & wPawns) == 0) {
            mgScore -= 15;
        }
    }

    if (bKingSq != SQ_NONE) {
        int f = squareFile(bKingSq);
        // Kingside pawn shield
        if (f >= 5) {
            int shield = __builtin_popcountll(bPawns & (fileMask[5] | fileMask[6] | fileMask[7]) & (rankMask[6] | rankMask[5]));
            mgScore -= (shield - 3) * 15;
        } else if (f <= 2) { // Queenside pawn shield
            int shield = __builtin_popcountll(bPawns & (fileMask[0] | fileMask[1] | fileMask[2]) & (rankMask[6] | rankMask[5]));
            mgScore -= (shield - 3) * 15;
        }
        // Open file on king
        if ((fileMask[f] & bPawns) == 0) {
            mgScore += 15;
        }
    }

    // ------------------------------------------------------------------------
    // Game Phase Tapered Interpolation
    // ------------------------------------------------------------------------
    // gamePhase ranges from 0 (Endgame: 2 Kings + pawns) to 24 (Startpos: full material)
    int mgPhase = std::min(24, gamePhase);
    int egPhase = 24 - mgPhase;

    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

} // namespace Evaluate
} // namespace Voraxis
