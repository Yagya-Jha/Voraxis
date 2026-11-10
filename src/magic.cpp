#include "magic.h"
#include "magic_numbers.inc"

namespace Voraxis {
namespace Magic {

static uint64_t bishopMasks[64];
static uint64_t rookMasks[64];
static int bishopShifts[64];
static int rookShifts[64];
static unsigned int bishopOffsets[64];
static unsigned int rookOffsets[64];

static uint64_t bishopTable[5248];
static uint64_t rookTable[102400];
static bool tablesInitialized = false;

static uint64_t computeRookMask(int sq) {
    uint64_t mask = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int curR = r + 1; curR < 7; ++curR) mask |= (1ULL << (curR * 8 + f));
    for (int curR = r - 1; curR > 0; --curR) mask |= (1ULL << (curR * 8 + f));
    for (int curF = f + 1; curF < 7; ++curF) mask |= (1ULL << (r * 8 + curF));
    for (int curF = f - 1; curF > 0; --curF) mask |= (1ULL << (r * 8 + curF));
    return mask;
}

static uint64_t computeBishopMask(int sq) {
    uint64_t mask = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1, cr = r + 1; cf < 7 && cr < 7; ++cf, ++cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f - 1, cr = r + 1; cf > 0 && cr < 7; --cf, ++cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f + 1, cr = r - 1; cf < 7 && cr > 0; ++cf, --cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f - 1, cr = r - 1; cf > 0 && cr > 0; --cf, --cr) mask |= (1ULL << (cr * 8 + cf));
    return mask;
}

static uint64_t computeRookAttacksSlow(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1; cf <= 7; ++cf) { attacks |= (1ULL << (r * 8 + cf)); if (occ & (1ULL << (r * 8 + cf))) break; }
    for (int cf = f - 1; cf >= 0; --cf) { attacks |= (1ULL << (r * 8 + cf)); if (occ & (1ULL << (r * 8 + cf))) break; }
    for (int cr = r + 1; cr <= 7; ++cr) { attacks |= (1ULL << (cr * 8 + f)); if (occ & (1ULL << (cr * 8 + f))) break; }
    for (int cr = r - 1; cr >= 0; --cr) { attacks |= (1ULL << (cr * 8 + f)); if (occ & (1ULL << (cr * 8 + f))) break; }
    return attacks;
}

static uint64_t computeBishopAttacksSlow(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1, cr = r + 1; cf <= 7 && cr <= 7; ++cf, ++cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f - 1, cr = r + 1; cf >= 0 && cr <= 7; --cf, ++cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f + 1, cr = r - 1; cf <= 7 && cr >= 0; ++cf, --cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f - 1, cr = r - 1; cf >= 0 && cr >= 0; --cf, --cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    return attacks;
}

void initMagicTables() {
    if (tablesInitialized) return;

    unsigned int bOffset = 0;
    for (int sq = 0; sq < 64; ++sq) {
        bishopMasks[sq] = computeBishopMask(sq);
        int bits = __builtin_popcountll(bishopMasks[sq]);
        bishopShifts[sq] = 64 - bits;
        bishopOffsets[sq] = bOffset;

        uint64_t mask = bishopMasks[sq];
        uint64_t current = 0ULL;
        do {
            unsigned int idx = (unsigned int)((current * BISHOP_MAGICS[sq]) >> bishopShifts[sq]);
            bishopTable[bOffset + idx] = computeBishopAttacksSlow(sq, current);
            current = (current - mask) & mask;
        } while (current);

        bOffset += (1 << bits);
    }

    unsigned int rOffset = 0;
    for (int sq = 0; sq < 64; ++sq) {
        rookMasks[sq] = computeRookMask(sq);
        int bits = __builtin_popcountll(rookMasks[sq]);
        rookShifts[sq] = 64 - bits;
        rookOffsets[sq] = rOffset;

        uint64_t mask = rookMasks[sq];
        uint64_t current = 0ULL;
        do {
            unsigned int idx = (unsigned int)((current * ROOK_MAGICS[sq]) >> rookShifts[sq]);
            rookTable[rOffset + idx] = computeRookAttacksSlow(sq, current);
            current = (current - mask) & mask;
        } while (current);

        rOffset += (1 << bits);
    }

    tablesInitialized = true;
}

uint64_t getBishopAttacks(int sq, uint64_t occ) {
    if (!tablesInitialized) initMagicTables();
    uint64_t blockers = occ & bishopMasks[sq];
    unsigned int idx = (unsigned int)((blockers * BISHOP_MAGICS[sq]) >> bishopShifts[sq]);
    return bishopTable[bishopOffsets[sq] + idx];
}

uint64_t getRookAttacks(int sq, uint64_t occ) {
    if (!tablesInitialized) initMagicTables();
    uint64_t blockers = occ & rookMasks[sq];
    unsigned int idx = (unsigned int)((blockers * ROOK_MAGICS[sq]) >> rookShifts[sq]);
    return rookTable[rookOffsets[sq] + idx];
}

uint64_t getQueenAttacks(int sq, uint64_t occ) {
    return getBishopAttacks(sq, occ) | getRookAttacks(sq, occ);
}

} // namespace Magic
} // namespace Voraxis
