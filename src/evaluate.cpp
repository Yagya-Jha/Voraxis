#include "evaluate.h"
#include "movegen.h"
#include <algorithm>

namespace Voraxis {
namespace Evaluate {

// ============================================================================
// PeSTO Piece-Square Tables (Rank 8 at index 0 down to Rank 1 at index 56)
// ============================================================================

// Pawns
static const int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0
};

static const int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      0,   0,   0,   0,   0,   0,   0,   0
};

// Knights
static const int mg_knight_table[64] = {
   -167, -89, -34, -49,  61, -97, -15, -107,
    -73, -41,  72,  36,  23,  62,   7,  -17,
    -47,  60,  37,  65,  84, 129,  73,   44,
     -9,  17,  19,  53,  37,  69,  18,   22,
    -13,   4,  16,  13,  28,  19,  21,   -8,
    -23,  -9,  12,  10,  19,  17,  25,  -16,
    -29, -53, -12,  -3,  -1,  18, -14,  -19,
   -105, -21, -58, -33, -17, -28, -19,  -23
};

static const int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64
};

// Bishops
static const int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};

static const int eg_bishop_table[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17
};

// Rooks
static const int mg_rook_table[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26
};

static const int eg_rook_table[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20
};

// Queens
static const int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};

static const int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  18,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41
};

// Kings (Middlegame: shelter on wings; Endgame: centralize)
static const int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

static const int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

// ============================================================================
// Pawn Structure & Board Lookup Bitboards (4.3 & 4.4)
// ============================================================================
constexpr uint64_t LIGHT_SQUARES_BB = 0x55AA55AA55AA55AAULL;
constexpr uint64_t DARK_SQUARES_BB = ~LIGHT_SQUARES_BB;
constexpr uint64_t CENTER_MASK = 0x00003C3C3C3C0000ULL;

static uint64_t fileMask[8];
static uint64_t rankMask[8];
static uint64_t adjacentFilesMask[8];
static uint64_t passedPawnMask[2][64];
static uint64_t pawnSupportMask[2][64];
static uint64_t pawnStopAttackMask[2][64];
static uint64_t outpostHoleMask[2][64];
static uint64_t ranksBehindAndEqual[2][8];
static bool tablesInitialized = false;

// Passed pawn rank bonuses (indexed by relative rank 0 to 7)
constexpr int PASSED_PAWN_MG[8] = { 0, 0, 10, 15, 35, 70, 120, 0 };
constexpr int PASSED_PAWN_EG[8] = { 0, 0, 15, 30, 60, 120, 200, 0 };

// Phalanx rank bonuses (indexed by relative rank 0 to 7)
constexpr int PHALANX_MG[8] = { 0, 0, 5, 12, 20, 35, 50, 0 };
constexpr int PHALANX_EG[8] = { 0, 0, 5, 15, 25, 45, 60, 0 };

void initEvaluation() {
    if (tablesInitialized) return;

    for (int f = 0; f < 8; ++f) {
        fileMask[f] = 0x0101010101010101ULL << f;
    }
    for (int r = 0; r < 8; ++r) {
        rankMask[r] = 0xFFULL << (r * 8);
    }
    for (int f = 0; f < 8; ++f) {
        adjacentFilesMask[f] = ((f > 0) ? fileMask[f - 1] : 0ULL) | ((f < 7) ? fileMask[f + 1] : 0ULL);
    }

    for (int r = 0; r < 8; ++r) {
        ranksBehindAndEqual[WHITE][r] = 0ULL;
        for (int i = 0; i <= r; ++i) {
            ranksBehindAndEqual[WHITE][r] |= rankMask[i];
        }
        ranksBehindAndEqual[BLACK][r] = 0ULL;
        for (int i = r; i < 8; ++i) {
            ranksBehindAndEqual[BLACK][r] |= rankMask[i];
        }
    }

    for (int sq = 0; sq < 64; ++sq) {
        int f = sq % 8;
        int r = sq / 8;

        // Passed pawn masks (ahead on same and adjacent files)
        passedPawnMask[WHITE][sq] = 0ULL;
        for (int r_ahead = r + 1; r_ahead < 8; ++r_ahead) {
            passedPawnMask[WHITE][sq] |= (1ULL << (r_ahead * 8 + f));
            if (f > 0) passedPawnMask[WHITE][sq] |= (1ULL << (r_ahead * 8 + f - 1));
            if (f < 7) passedPawnMask[WHITE][sq] |= (1ULL << (r_ahead * 8 + f + 1));
        }

        passedPawnMask[BLACK][sq] = 0ULL;
        for (int r_ahead = r - 1; r_ahead >= 0; --r_ahead) {
            passedPawnMask[BLACK][sq] |= (1ULL << (r_ahead * 8 + f));
            if (f > 0) passedPawnMask[BLACK][sq] |= (1ULL << (r_ahead * 8 + f - 1));
            if (f < 7) passedPawnMask[BLACK][sq] |= (1ULL << (r_ahead * 8 + f + 1));
        }

        // Pawn support masks (squares from which friendly pawns can protect sq)
        pawnSupportMask[WHITE][sq] = 0ULL;
        if (r > 0) {
            if (f > 0) pawnSupportMask[WHITE][sq] |= (1ULL << ((r - 1) * 8 + f - 1));
            if (f < 7) pawnSupportMask[WHITE][sq] |= (1ULL << ((r - 1) * 8 + f + 1));
        }

        pawnSupportMask[BLACK][sq] = 0ULL;
        if (r < 7) {
            if (f > 0) pawnSupportMask[BLACK][sq] |= (1ULL << ((r + 1) * 8 + f - 1));
            if (f < 7) pawnSupportMask[BLACK][sq] |= (1ULL << ((r + 1) * 8 + f + 1));
        }

        // Backward pawn stop attack mask (enemy pawns controlling the stop square in front of sq)
        pawnStopAttackMask[WHITE][sq] = 0ULL;
        if (r + 1 < 8) {
            int stop = (r + 1) * 8 + f;
            int stop_r = stop / 8;
            int stop_f = stop % 8;
            if (stop_r + 1 < 8) {
                if (stop_f > 0) pawnStopAttackMask[WHITE][sq] |= (1ULL << ((stop_r + 1) * 8 + stop_f - 1));
                if (stop_f < 7) pawnStopAttackMask[WHITE][sq] |= (1ULL << ((stop_r + 1) * 8 + stop_f + 1));
            }
        }

        pawnStopAttackMask[BLACK][sq] = 0ULL;
        if (r - 1 >= 0) {
            int stop = (r - 1) * 8 + f;
            int stop_r = stop / 8;
            int stop_f = stop % 8;
            if (stop_r - 1 >= 0) {
                if (stop_f > 0) pawnStopAttackMask[BLACK][sq] |= (1ULL << ((stop_r - 1) * 8 + stop_f - 1));
                if (stop_f < 7) pawnStopAttackMask[BLACK][sq] |= (1ULL << ((stop_r - 1) * 8 + stop_f + 1));
            }
        }

        // 4.4 Outpost hole masks (enemy pawns on adjacent files that could attack or advance to attack sq)
        outpostHoleMask[WHITE][sq] = 0ULL;
        for (int r_opp = r; r_opp < 8; ++r_opp) {
            if (f > 0) outpostHoleMask[WHITE][sq] |= (1ULL << (r_opp * 8 + f - 1));
            if (f < 7) outpostHoleMask[WHITE][sq] |= (1ULL << (r_opp * 8 + f + 1));
        }

        outpostHoleMask[BLACK][sq] = 0ULL;
        for (int r_opp = r; r_opp >= 0; --r_opp) {
            if (f > 0) outpostHoleMask[BLACK][sq] |= (1ULL << (r_opp * 8 + f - 1));
            if (f < 7) outpostHoleMask[BLACK][sq] |= (1ULL << (r_opp * 8 + f + 1));
        }
    }

    tablesInitialized = true;
}

// 4.3 Pawn Structure Evaluation: Doubled, Isolated, Backward, Passed & Connected
void evaluatePawnStructure(const Board& board, int& mgScore, int& egScore) {
    uint64_t wPawns = board.pieceBB[W_PAWN];
    uint64_t bPawns = board.pieceBB[B_PAWN];

    // 1. Doubled Pawns (evaluated per file)
    for (int f = 0; f < 8; ++f) {
        int wCount = __builtin_popcountll(wPawns & fileMask[f]);
        if (wCount > 1) {
            mgScore -= (wCount - 1) * 15;
            egScore -= (wCount - 1) * 25;
        }
        int bCount = __builtin_popcountll(bPawns & fileMask[f]);
        if (bCount > 1) {
            mgScore += (bCount - 1) * 15;
            egScore += (bCount - 1) * 25;
        }
    }

    // 2. White Pawns: Isolated, Backward, Passed, Connected
    uint64_t wPawnsCopy = wPawns;
    uint64_t wPassedPawns = 0ULL;
    while (wPawnsCopy) {
        int sq = __builtin_ctzll(wPawnsCopy);
        wPawnsCopy &= wPawnsCopy - 1;
        int f = sq % 8;
        int r = sq / 8;

        // Isolated pawn (no friendly pawns on adjacent files)
        if ((wPawns & adjacentFilesMask[f]) == 0) {
            bool openFile = (bPawns & fileMask[f]) == 0;
            mgScore -= openFile ? 20 : 15;
            egScore -= openFile ? 25 : 20;
        }
        // Backward pawn (not isolated, lagging behind neighbors, stop square attacked by enemy)
        else if ((wPawns & adjacentFilesMask[f] & ranksBehindAndEqual[WHITE][r]) == 0) {
            if (bPawns & pawnStopAttackMask[WHITE][sq]) {
                mgScore -= 14;
                egScore -= 18;
            }
        }

        // Passed pawn (no enemy pawns ahead on same or adjacent files)
        if ((bPawns & passedPawnMask[WHITE][sq]) == 0) {
            wPassedPawns |= (1ULL << sq);
            mgScore += PASSED_PAWN_MG[r];
            egScore += PASSED_PAWN_EG[r];

            // Protected passed pawn bonus
            if (wPawns & pawnSupportMask[WHITE][sq]) {
                mgScore += 15;
                egScore += 25;
            }
        } else {
            // Connected non-passed pawn bonus
            if (wPawns & pawnSupportMask[WHITE][sq]) {
                mgScore += 8;
                egScore += 12;
            }
        }
    }

    // Connected passed pawns bonus (White)
    uint64_t wPassCopy = wPassedPawns;
    while (wPassCopy) {
        int sq = __builtin_ctzll(wPassCopy);
        wPassCopy &= wPassCopy - 1;
        int f = sq % 8;
        if (wPassedPawns & adjacentFilesMask[f]) {
            mgScore += 15;
            egScore += 30;
        }
    }

    // 3. Black Pawns: Isolated, Backward, Passed, Connected
    uint64_t bPawnsCopy = bPawns;
    uint64_t bPassedPawns = 0ULL;
    while (bPawnsCopy) {
        int sq = __builtin_ctzll(bPawnsCopy);
        bPawnsCopy &= bPawnsCopy - 1;
        int f = sq % 8;
        int r = sq / 8;
        int relRank = 7 - r;

        // Isolated pawn
        if ((bPawns & adjacentFilesMask[f]) == 0) {
            bool openFile = (wPawns & fileMask[f]) == 0;
            mgScore += openFile ? 20 : 15;
            egScore += openFile ? 25 : 20;
        }
        // Backward pawn
        else if ((bPawns & adjacentFilesMask[f] & ranksBehindAndEqual[BLACK][r]) == 0) {
            if (wPawns & pawnStopAttackMask[BLACK][sq]) {
                mgScore -= 14;
                egScore -= 18;
            }
        }

        // Passed pawn
        if ((wPawns & passedPawnMask[BLACK][sq]) == 0) {
            bPassedPawns |= (1ULL << sq);
            mgScore -= PASSED_PAWN_MG[relRank];
            egScore -= PASSED_PAWN_EG[relRank];

            // Protected passed pawn bonus
            if (bPawns & pawnSupportMask[BLACK][sq]) {
                mgScore -= 15;
                egScore -= 25;
            }
        } else {
            // Connected non-passed pawn bonus
            if (bPawns & pawnSupportMask[BLACK][sq]) {
                mgScore -= 8;
                egScore -= 12;
            }
        }
    }

    // Connected passed pawns bonus (Black)
    uint64_t bPassCopy = bPassedPawns;
    while (bPassCopy) {
        int sq = __builtin_ctzll(bPassCopy);
        bPassCopy &= bPassCopy - 1;
        int f = sq % 8;
        if (bPassedPawns & adjacentFilesMask[f]) {
            mgScore -= 15;
            egScore -= 30;
        }
    }

    // 4. Pawn Phalanxes (pawns side-by-side on adjacent files on the same rank)
    uint64_t wPhalanxes = wPawns & ((wPawns & ~fileMask[7]) << 1);
    while (wPhalanxes) {
        int sq = __builtin_ctzll(wPhalanxes);
        wPhalanxes &= wPhalanxes - 1;
        int r = sq / 8;
        mgScore += PHALANX_MG[r];
        egScore += PHALANX_EG[r];
    }

    uint64_t bPhalanxes = bPawns & ((bPawns & ~fileMask[7]) << 1);
    while (bPhalanxes) {
        int sq = __builtin_ctzll(bPhalanxes);
        bPhalanxes &= bPhalanxes - 1;
        int r = sq / 8;
        int relRank = 7 - r;
        mgScore -= PHALANX_MG[relRank];
        egScore -= PHALANX_EG[relRank];
    }
}

// 4.4 Piece Activity & Coordination: Bishop Pair, Rooks on Open/7th, Outposts, Bad Bishops
void evaluatePieceActivity(const Board& board, int& mgScore, int& egScore) {
    uint64_t wPawns = board.pieceBB[W_PAWN];
    uint64_t bPawns = board.pieceBB[B_PAWN];
    uint64_t occAll = board.occBB[WHITE] | board.occBB[BLACK];

    // 1. Bishop Pair Bonus
    int wBishopsCount = __builtin_popcountll(board.pieceBB[W_BISHOP]);
    if (wBishopsCount >= 2) {
        mgScore += 30;
        egScore += 50;
    }
    int bBishopsCount = __builtin_popcountll(board.pieceBB[B_BISHOP]);
    if (bBishopsCount >= 2) {
        mgScore -= 30;
        egScore -= 50;
    }

    // 2. Rooks on Open & Semi-Open Files, and on 7th Rank
    uint64_t wRooks = board.pieceBB[W_ROOK];
    int wRooksOn7th = 0;
    while (wRooks) {
        int sq = __builtin_ctzll(wRooks);
        wRooks &= wRooks - 1;
        int f = sq % 8;
        int r = sq / 8;

        // Open and Semi-Open file
        if ((wPawns & fileMask[f]) == 0) {
            if ((bPawns & fileMask[f]) == 0) {
                // Open file
                mgScore += 30;
                egScore += 20;
            } else {
                // Semi-open file
                mgScore += 15;
                egScore += 10;
            }
        }

        // Rook on 7th rank (attacking pawns or pinning king)
        if (r == 6) {
            if ((board.pieceBB[B_KING] & (rankMask[6] | rankMask[7])) || (bPawns & rankMask[6])) {
                mgScore += 25;
                egScore += 40;
                wRooksOn7th++;
            }
        }
    }
    if (wRooksOn7th >= 2) {
        // Doubled rooks on 7th rank
        mgScore += 20;
        egScore += 30;
    }

    uint64_t bRooks = board.pieceBB[B_ROOK];
    int bRooksOn2nd = 0;
    while (bRooks) {
        int sq = __builtin_ctzll(bRooks);
        bRooks &= bRooks - 1;
        int f = sq % 8;
        int r = sq / 8;

        // Open and Semi-Open file
        if ((bPawns & fileMask[f]) == 0) {
            if ((wPawns & fileMask[f]) == 0) {
                // Open file
                mgScore -= 30;
                egScore -= 20;
            } else {
                // Semi-open file
                mgScore -= 15;
                egScore -= 10;
            }
        }

        // Rook on 2nd rank (White's 2nd rank from Black perspective)
        if (r == 1) {
            if ((board.pieceBB[W_KING] & (rankMask[0] | rankMask[1])) || (wPawns & rankMask[1])) {
                mgScore -= 25;
                egScore -= 40;
                bRooksOn2nd++;
            }
        }
    }
    if (bRooksOn2nd >= 2) {
        mgScore -= 20;
        egScore -= 30;
    }

    // 3. Knight Outposts on Central Protected Squares
    uint64_t wKnights = board.pieceBB[W_KNIGHT];
    while (wKnights) {
        int sq = __builtin_ctzll(wKnights);
        wKnights &= wKnights - 1;
        int f = sq % 8;
        int r = sq / 8;

        // Central files (c, d, e, f) and advanced ranks (4, 5, 6)
        if (f >= 2 && f <= 5 && r >= 3 && r <= 5) {
            if (wPawns & pawnSupportMask[WHITE][sq]) {
                // Supported by friendly pawn
                if ((bPawns & outpostHoleMask[WHITE][sq]) == 0) {
                    // Outpost hole: cannot be driven away by enemy pawn
                    int bonusMG = (r >= 4) ? 30 : 15;
                    int bonusEG = (r >= 4) ? 20 : 10;
                    mgScore += bonusMG;
                    egScore += bonusEG;
                }
            }
        }
    }

    uint64_t bKnights = board.pieceBB[B_KNIGHT];
    while (bKnights) {
        int sq = __builtin_ctzll(bKnights);
        bKnights &= bKnights - 1;
        int f = sq % 8;
        int r = sq / 8;
        int relRank = 7 - r;

        if (f >= 2 && f <= 5 && relRank >= 3 && relRank <= 5) {
            if (bPawns & pawnSupportMask[BLACK][sq]) {
                if ((wPawns & outpostHoleMask[BLACK][sq]) == 0) {
                    int bonusMG = (relRank >= 4) ? 30 : 15;
                    int bonusEG = (relRank >= 4) ? 20 : 10;
                    mgScore -= bonusMG;
                    egScore -= bonusEG;
                }
            }
        }
    }

    // 4. Bad Bishops Blocked by Friendly Pawns & Mobility
    uint64_t wBishops = board.pieceBB[W_BISHOP];
    while (wBishops) {
        int sq = __builtin_ctzll(wBishops);
        wBishops &= wBishops - 1;

        bool isLight = ((1ULL << sq) & LIGHT_SQUARES_BB) != 0;
        int sameColorPawns = __builtin_popcountll(wPawns & (isLight ? LIGHT_SQUARES_BB : DARK_SQUARES_BB) & CENTER_MASK);
        if (sameColorPawns >= 2) {
            mgScore -= (sameColorPawns - 1) * 8;
            egScore -= (sameColorPawns - 1) * 12;
        }

        int mob = __builtin_popcountll(Movegen::getBishopAttacks(sq, occAll) & ~board.occBB[WHITE]);
        if (mob <= 3) {
            mgScore -= 15;
            egScore -= 20;
        } else if (mob >= 8) {
            mgScore += 10;
            egScore += 15;
        }
    }

    uint64_t bBishops = board.pieceBB[B_BISHOP];
    while (bBishops) {
        int sq = __builtin_ctzll(bBishops);
        bBishops &= bBishops - 1;

        bool isLight = ((1ULL << sq) & LIGHT_SQUARES_BB) != 0;
        int sameColorPawns = __builtin_popcountll(bPawns & (isLight ? LIGHT_SQUARES_BB : DARK_SQUARES_BB) & CENTER_MASK);
        if (sameColorPawns >= 2) {
            mgScore += (sameColorPawns - 1) * 8;
            egScore += (sameColorPawns - 1) * 12;
        }

        int mob = __builtin_popcountll(Movegen::getBishopAttacks(sq, occAll) & ~board.occBB[BLACK]);
        if (mob <= 3) {
            mgScore += 15;
            egScore += 20;
        } else if (mob >= 8) {
            mgScore -= 10;
            egScore -= 15;
        }
    }
}

// 4.5 King Danger Non-Linear Table (derived from attack unit weights)
static const int kingDangerTable[100] = {
      0,   0,   2,   5,   8,  12,  16,  20,  25,  30,
     36,  42,  48,  55,  62,  70,  78,  86,  95, 104,
    114, 124, 134, 145, 156, 168, 180, 192, 205, 218,
    232, 246, 260, 275, 290, 305, 321, 337, 354, 371,
    388, 406, 424, 442, 461, 480, 500, 520, 540, 560,
    580, 600, 620, 640, 660, 680, 700, 720, 740, 760,
    780, 800, 820, 840, 860, 880, 900, 920, 940, 960,
    980, 1000, 1020, 1040, 1060, 1080, 1100, 1120, 1140, 1160,
    1180, 1200, 1220, 1240, 1260, 1280, 1300, 1320, 1340, 1360,
    1380, 1400, 1420, 1440, 1460, 1480, 1500, 1520, 1540, 1560
};

// 4.5 King Safety Evaluation: Pawn Shelter, Open/Semi-Open Files, and Virtual Attackers
void evaluateKingSafety(const Board& board, int& mgScore, int& egScore) {
    uint64_t wPawns = board.pieceBB[W_PAWN];
    uint64_t bPawns = board.pieceBB[B_PAWN];
    uint64_t occAll = board.occBB[WHITE] | board.occBB[BLACK];

    // ========================================================================
    // White King Safety
    // ========================================================================
    if (board.pieceBB[W_KING]) {
        int wKingSq = __builtin_ctzll(board.pieceBB[W_KING]);
        int kFile = wKingSq % 8;
        int kRank = wKingSq / 8;

        // 1. Pawn Shelter Shield (applicable when king is on ranks 1-3)
        if (kRank <= 2) {
            int minF = std::max(0, kFile - 1);
            int maxF = std::min(7, kFile + 1);
            for (int f = minF; f <= maxF; ++f) {
                uint64_t filePawns = wPawns & fileMask[f];
                if (!filePawns) {
                    mgScore -= 35; // Missing pawn shelter
                } else {
                    int pRank = __builtin_ctzll(filePawns) / 8;
                    if (pRank == 1) {
                        // Unmoved shelter on rank 2: no penalty
                    } else if (pRank == 2) {
                        mgScore -= 10; // Advanced 1 square (rank 3)
                    } else if (pRank == 3) {
                        mgScore -= 22; // Advanced to rank 4
                    } else {
                        mgScore -= 35; // Far advanced, no cover
                    }
                }

                // 2. Open / Semi-open files towards king
                if ((wPawns & fileMask[f]) == 0) {
                    if ((bPawns & fileMask[f]) == 0) {
                        // Open file directly pointing at king
                        mgScore -= 25;
                        if ((board.pieceBB[B_ROOK] | board.pieceBB[B_QUEEN]) & fileMask[f]) {
                            mgScore -= 20; // Enemy heavy piece on the file
                        }
                    } else {
                        // Semi-open file towards king
                        mgScore -= 15;
                        if ((board.pieceBB[B_ROOK] | board.pieceBB[B_QUEEN]) & fileMask[f]) {
                            mgScore -= 15;
                        }
                    }
                }
            }
        }

        // 3. Virtual Attacker Count & King Zone Danger (Middlegame)
        uint64_t wKingZone = Movegen::getKingAttacks(wKingSq) | (1ULL << wKingSq);
        if (kRank <= 1) {
            wKingZone |= (wKingZone << 8); // Add forward rank
        }

        int attackerCount = 0;
        int attackWeight = 0;

        uint64_t bKnights = board.pieceBB[B_KNIGHT];
        while (bKnights) {
            int sq = __builtin_ctzll(bKnights);
            bKnights &= bKnights - 1;
            int hits = __builtin_popcountll(Movegen::getKnightAttacks(sq) & wKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 2 * hits;
            }
        }

        uint64_t bBishops = board.pieceBB[B_BISHOP];
        while (bBishops) {
            int sq = __builtin_ctzll(bBishops);
            bBishops &= bBishops - 1;
            int hits = __builtin_popcountll(Movegen::getBishopAttacks(sq, occAll) & wKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 2 * hits;
            }
        }

        uint64_t bRooks = board.pieceBB[B_ROOK];
        while (bRooks) {
            int sq = __builtin_ctzll(bRooks);
            bRooks &= bRooks - 1;
            int hits = __builtin_popcountll(Movegen::getRookAttacks(sq, occAll) & wKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 3 * hits;
            }
        }

        uint64_t bQueens = board.pieceBB[B_QUEEN];
        while (bQueens) {
            int sq = __builtin_ctzll(bQueens);
            bQueens &= bQueens - 1;
            int hits = __builtin_popcountll(Movegen::getQueenAttacks(sq, occAll) & wKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 5 * hits;
            }
        }

        if (attackerCount >= 2) {
            int dangerIndex = std::min(99, attackWeight * 3);
            if (!board.pieceBB[B_QUEEN]) {
                dangerIndex /= 2; // Without an enemy queen, attacks are much less lethal
            }
            mgScore -= kingDangerTable[dangerIndex];
        }
    }

    // ========================================================================
    // Black King Safety
    // ========================================================================
    if (board.pieceBB[B_KING]) {
        int bKingSq = __builtin_ctzll(board.pieceBB[B_KING]);
        int kFile = bKingSq % 8;
        int kRank = bKingSq / 8;

        // 1. Pawn Shelter Shield (applicable when king is on ranks 6-8)
        if (kRank >= 5) {
            int minF = std::max(0, kFile - 1);
            int maxF = std::min(7, kFile + 1);
            for (int f = minF; f <= maxF; ++f) {
                uint64_t filePawns = bPawns & fileMask[f];
                if (!filePawns) {
                    mgScore += 35; // Missing pawn shelter for Black
                } else {
                    int pRank = (63 - __builtin_clzll(filePawns)) / 8;
                    if (pRank == 6) {
                        // Unmoved shelter on rank 7: no penalty
                    } else if (pRank == 5) {
                        mgScore += 10; // Advanced 1 square (rank 6)
                    } else if (pRank == 4) {
                        mgScore += 22; // Advanced to rank 5
                    } else {
                        mgScore += 35; // Far advanced, no cover
                    }
                }

                // 2. Open / Semi-open files towards king
                if ((bPawns & fileMask[f]) == 0) {
                    if ((wPawns & fileMask[f]) == 0) {
                        // Open file directly pointing at Black king
                        mgScore += 25;
                        if ((board.pieceBB[W_ROOK] | board.pieceBB[W_QUEEN]) & fileMask[f]) {
                            mgScore += 20;
                        }
                    } else {
                        // Semi-open file towards Black king
                        mgScore += 15;
                        if ((board.pieceBB[W_ROOK] | board.pieceBB[W_QUEEN]) & fileMask[f]) {
                            mgScore += 15;
                        }
                    }
                }
            }
        }

        // 3. Virtual Attacker Count & King Zone Danger (Middlegame)
        uint64_t bKingZone = Movegen::getKingAttacks(bKingSq) | (1ULL << bKingSq);
        if (kRank >= 6) {
            bKingZone |= (bKingZone >> 8); // Add forward rank towards White
        }

        int attackerCount = 0;
        int attackWeight = 0;

        uint64_t wKnights = board.pieceBB[W_KNIGHT];
        while (wKnights) {
            int sq = __builtin_ctzll(wKnights);
            wKnights &= wKnights - 1;
            int hits = __builtin_popcountll(Movegen::getKnightAttacks(sq) & bKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 2 * hits;
            }
        }

        uint64_t wBishops = board.pieceBB[W_BISHOP];
        while (wBishops) {
            int sq = __builtin_ctzll(wBishops);
            wBishops &= wBishops - 1;
            int hits = __builtin_popcountll(Movegen::getBishopAttacks(sq, occAll) & bKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 2 * hits;
            }
        }

        uint64_t wRooks = board.pieceBB[W_ROOK];
        while (wRooks) {
            int sq = __builtin_ctzll(wRooks);
            wRooks &= wRooks - 1;
            int hits = __builtin_popcountll(Movegen::getRookAttacks(sq, occAll) & bKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 3 * hits;
            }
        }

        uint64_t wQueens = board.pieceBB[W_QUEEN];
        while (wQueens) {
            int sq = __builtin_ctzll(wQueens);
            wQueens &= wQueens - 1;
            int hits = __builtin_popcountll(Movegen::getQueenAttacks(sq, occAll) & bKingZone);
            if (hits > 0) {
                attackerCount++;
                attackWeight += 5 * hits;
            }
        }

        if (attackerCount >= 2) {
            int dangerIndex = std::min(99, attackWeight * 3);
            if (!board.pieceBB[W_QUEEN]) {
                dangerIndex /= 2;
            }
            mgScore += kingDangerTable[dangerIndex];
        }
    }
}

int getGamePhase(const Board& board) {
    int phase = 0;
    phase += __builtin_popcountll(board.pieceBB[W_KNIGHT] | board.pieceBB[B_KNIGHT]) * 1;
    phase += __builtin_popcountll(board.pieceBB[W_BISHOP] | board.pieceBB[B_BISHOP]) * 1;
    phase += __builtin_popcountll(board.pieceBB[W_ROOK]   | board.pieceBB[B_ROOK])   * 2;
    phase += __builtin_popcountll(board.pieceBB[W_QUEEN]  | board.pieceBB[B_QUEEN])  * 4;
    return std::min(phase, MAX_PHASE);
}

int evaluate(const Board& board) {
    if (!tablesInitialized) {
        initEvaluation();
    }

    int mgScore = 0;
    int egScore = 0;

    // ------------------------------------------------------------------------
    // White Pieces
    // ------------------------------------------------------------------------
    uint64_t wPawns = board.pieceBB[W_PAWN];
    while (wPawns) {
        int sq = __builtin_ctzll(wPawns);
        wPawns &= wPawns - 1;
        mgScore += MG_PAWN + mg_pawn_table[sq ^ 56];
        egScore += EG_PAWN + eg_pawn_table[sq ^ 56];
    }

    uint64_t wKnights = board.pieceBB[W_KNIGHT];
    while (wKnights) {
        int sq = __builtin_ctzll(wKnights);
        wKnights &= wKnights - 1;
        mgScore += MG_KNIGHT + mg_knight_table[sq ^ 56];
        egScore += EG_KNIGHT + eg_knight_table[sq ^ 56];
    }

    uint64_t wBishops = board.pieceBB[W_BISHOP];
    while (wBishops) {
        int sq = __builtin_ctzll(wBishops);
        wBishops &= wBishops - 1;
        mgScore += MG_BISHOP + mg_bishop_table[sq ^ 56];
        egScore += EG_BISHOP + eg_bishop_table[sq ^ 56];
    }

    uint64_t wRooks = board.pieceBB[W_ROOK];
    while (wRooks) {
        int sq = __builtin_ctzll(wRooks);
        wRooks &= wRooks - 1;
        mgScore += MG_ROOK + mg_rook_table[sq ^ 56];
        egScore += EG_ROOK + eg_rook_table[sq ^ 56];
    }

    uint64_t wQueens = board.pieceBB[W_QUEEN];
    while (wQueens) {
        int sq = __builtin_ctzll(wQueens);
        wQueens &= wQueens - 1;
        mgScore += MG_QUEEN + mg_queen_table[sq ^ 56];
        egScore += EG_QUEEN + eg_queen_table[sq ^ 56];
    }

    if (board.pieceBB[W_KING]) {
        int sq = __builtin_ctzll(board.pieceBB[W_KING]);
        mgScore += mg_king_table[sq ^ 56];
        egScore += eg_king_table[sq ^ 56];
    }

    // ------------------------------------------------------------------------
    // Black Pieces
    // ------------------------------------------------------------------------
    uint64_t bPawns = board.pieceBB[B_PAWN];
    while (bPawns) {
        int sq = __builtin_ctzll(bPawns);
        bPawns &= bPawns - 1;
        mgScore -= (MG_PAWN + mg_pawn_table[sq]);
        egScore -= (EG_PAWN + eg_pawn_table[sq]);
    }

    uint64_t bKnights = board.pieceBB[B_KNIGHT];
    while (bKnights) {
        int sq = __builtin_ctzll(bKnights);
        bKnights &= bKnights - 1;
        mgScore -= (MG_KNIGHT + mg_knight_table[sq]);
        egScore -= (EG_KNIGHT + eg_knight_table[sq]);
    }

    uint64_t bBishops = board.pieceBB[B_BISHOP];
    while (bBishops) {
        int sq = __builtin_ctzll(bBishops);
        bBishops &= bBishops - 1;
        mgScore -= (MG_BISHOP + mg_bishop_table[sq]);
        egScore -= (EG_BISHOP + eg_bishop_table[sq]);
    }

    uint64_t bRooks = board.pieceBB[B_ROOK];
    while (bRooks) {
        int sq = __builtin_ctzll(bRooks);
        bRooks &= bRooks - 1;
        mgScore -= (MG_ROOK + mg_rook_table[sq]);
        egScore -= (EG_ROOK + eg_rook_table[sq]);
    }

    uint64_t bQueens = board.pieceBB[B_QUEEN];
    while (bQueens) {
        int sq = __builtin_ctzll(bQueens);
        bQueens &= bQueens - 1;
        mgScore -= (MG_QUEEN + mg_queen_table[sq]);
        egScore -= (EG_QUEEN + eg_queen_table[sq]);
    }

    if (board.pieceBB[B_KING]) {
        int sq = __builtin_ctzll(board.pieceBB[B_KING]);
        mgScore -= mg_king_table[sq];
        egScore -= eg_king_table[sq];
    }

    // ------------------------------------------------------------------------
    // 4.3 Pawn Structure Evaluation
    // ------------------------------------------------------------------------
    evaluatePawnStructure(board, mgScore, egScore);

    // ------------------------------------------------------------------------
    // 4.4 Piece Activity & Coordination Evaluation
    // ------------------------------------------------------------------------
    evaluatePieceActivity(board, mgScore, egScore);

    // ------------------------------------------------------------------------
    // 4.5 King Safety Evaluation
    // ------------------------------------------------------------------------
    evaluateKingSafety(board, mgScore, egScore);

    // ------------------------------------------------------------------------
    // Tapered Interpolation by Game Phase (4.2)
    // ------------------------------------------------------------------------
    int phase = getGamePhase(board);
    int score = (mgScore * phase + egScore * (MAX_PHASE - phase)) / MAX_PHASE;
    return score;
}

} // namespace Evaluate
} // namespace Voraxis
