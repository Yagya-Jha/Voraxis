#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <chrono>

// High quality 64-bit PRNG (splitmix64)
uint64_t rngState = 12345678901234567ULL;
uint64_t nextU64() {
    uint64_t z = (rngState += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t randomCandidate() {
    return nextU64() & nextU64() & nextU64();
}

int countBits(uint64_t b) {
    return __builtin_popcountll(b);
}

uint64_t rookMask(int sq) {
    uint64_t mask = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int curR = r + 1; curR < 7; ++curR) mask |= (1ULL << (curR * 8 + f));
    for (int curR = r - 1; curR > 0; --curR) mask |= (1ULL << (curR * 8 + f));
    for (int curF = f + 1; curF < 7; ++curF) mask |= (1ULL << (r * 8 + curF));
    for (int curF = f - 1; curF > 0; --curF) mask |= (1ULL << (r * 8 + curF));
    return mask;
}

uint64_t bishopMask(int sq) {
    uint64_t mask = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1, cr = r + 1; cf < 7 && cr < 7; ++cf, ++cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f - 1, cr = r + 1; cf > 0 && cr < 7; --cf, ++cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f + 1, cr = r - 1; cf < 7 && cr > 0; ++cf, --cr) mask |= (1ULL << (cr * 8 + cf));
    for (int cf = f - 1, cr = r - 1; cf > 0 && cr > 0; --cf, --cr) mask |= (1ULL << (cr * 8 + cf));
    return mask;
}

uint64_t rookAttacksSlow(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1; cf <= 7; ++cf) { attacks |= (1ULL << (r * 8 + cf)); if (occ & (1ULL << (r * 8 + cf))) break; }
    for (int cf = f - 1; cf >= 0; --cf) { attacks |= (1ULL << (r * 8 + cf)); if (occ & (1ULL << (r * 8 + cf))) break; }
    for (int cr = r + 1; cr <= 7; ++cr) { attacks |= (1ULL << (cr * 8 + f)); if (occ & (1ULL << (cr * 8 + f))) break; }
    for (int cr = r - 1; cr >= 0; --cr) { attacks |= (1ULL << (cr * 8 + f)); if (occ & (1ULL << (cr * 8 + f))) break; }
    return attacks;
}

uint64_t bishopAttacksSlow(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;
    for (int cf = f + 1, cr = r + 1; cf <= 7 && cr <= 7; ++cf, ++cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f - 1, cr = r + 1; cf >= 0 && cr <= 7; --cf, ++cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f + 1, cr = r - 1; cf <= 7 && cr >= 0; ++cf, --cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    for (int cf = f - 1, cr = r - 1; cf >= 0 && cr >= 0; --cf, --cr) { attacks |= (1ULL << (cr * 8 + cf)); if (occ & (1ULL << (cr * 8 + cf))) break; }
    return attacks;
}

uint64_t findMagic(int sq, bool isRook, int bits) {
    uint64_t mask = isRook ? rookMask(sq) : bishopMask(sq);
    int numSubsets = 1 << bits;
    std::vector<uint64_t> occupancies(numSubsets);
    std::vector<uint64_t> attacks(numSubsets);
    std::vector<uint64_t> table(numSubsets, 0ULL);
    std::vector<int> used(numSubsets, 0);

    uint64_t current = 0ULL;
    int count = 0;
    do {
        occupancies[count] = current;
        attacks[count] = isRook ? rookAttacksSlow(sq, current) : bishopAttacksSlow(sq, current);
        count++;
        current = (current - mask) & mask;
    } while (current);

    int shift = 64 - bits;
    int epoch = 0;

    for (int attempt = 0; attempt < 100000000; ++attempt) {
        uint64_t magic = randomCandidate();
        epoch++;
        bool fail = false;
        for (int i = 0; i < numSubsets; ++i) {
            unsigned int idx = (unsigned int)((occupancies[i] * magic) >> shift);
            if (used[idx] == epoch) {
                if (table[idx] != attacks[i]) {
                    fail = true;
                    break;
                }
            } else {
                used[idx] = epoch;
                table[idx] = attacks[i];
            }
        }
        if (!fail) return magic;
    }
    std::cerr << "Fatal: Magic not found for sq " << sq << std::endl;
    return 0ULL;
}

int main() {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::cout << "Finding 64 bishop magics..." << std::flush;
    uint64_t bishopMagics[64];
    for (int sq = 0; sq < 64; ++sq) {
        int bits = countBits(bishopMask(sq));
        bishopMagics[sq] = findMagic(sq, false, bits);
    }
    std::cout << " Done!\nFinding 64 rook magics..." << std::flush;
    uint64_t rookMagics[64];
    for (int sq = 0; sq < 64; ++sq) {
        int bits = countBits(rookMask(sq));
        rookMagics[sq] = findMagic(sq, true, bits);
        std::cout << "." << std::flush;
    }
    std::cout << " Done!\n";

    std::ofstream out("src/magic_numbers.inc");
    out << "// Auto-generated verified 64-bit magic bitboard constants for Voraxis\n";
    out << "#pragma once\n#include <cstdint>\n\n";
    out << "constexpr uint64_t BISHOP_MAGICS[64] = {\n";
    for (int i = 0; i < 64; i += 4) {
        out << "    ";
        for (int j = 0; j < 4; ++j) {
            out << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << bishopMagics[i + j] << "ULL";
            if (i + j < 63) out << ", ";
        }
        out << "\n";
    }
    out << "};\n\n";

    out << "constexpr uint64_t ROOK_MAGICS[64] = {\n";
    for (int i = 0; i < 64; i += 4) {
        out << "    ";
        for (int j = 0; j < 4; ++j) {
            out << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << rookMagics[i + j] << "ULL";
            if (i + j < 63) out << ", ";
        }
        out << "\n";
    }
    out << "};\n";
    out.close();

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "src/magic_numbers.inc generated in " << ms << " ms!\n";
    return 0;
}
