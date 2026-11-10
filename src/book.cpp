#include "book.h"
#include "polyglot_keys.h"
#include "movegen.h"
#include <fstream>
#include <algorithm>
#include <random>
#include <iostream>

namespace Voraxis {
namespace Book {

#pragma pack(push, 1)
struct RawEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};
#pragma pack(pop)

static_assert(sizeof(RawEntry) == 16, "RawEntry must be exactly 16 bytes");

struct BookEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
};

static std::vector<BookEntry> s_bookEntries;
static bool s_loaded = false;
static std::mt19937 s_rng(1337);

bool init(const std::string& bookPath) {
    std::string path = bookPath;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        path = "../" + bookPath;
        file.open(path, std::ios::binary | std::ios::ate);
    }
    if (!file.is_open()) {
        s_loaded = false;
        return false;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0 || (fileSize % sizeof(RawEntry)) != 0) {
        file.close();
        s_loaded = false;
        return false;
    }

    size_t numEntries = fileSize / sizeof(RawEntry);
    std::vector<RawEntry> rawBuffer(numEntries);

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(rawBuffer.data()), fileSize);
    file.close();

    s_bookEntries.resize(numEntries);
    for (size_t i = 0; i < numEntries; ++i) {
        s_bookEntries[i].key = __builtin_bswap64(rawBuffer[i].key);
        s_bookEntries[i].move = __builtin_bswap16(rawBuffer[i].move);
        s_bookEntries[i].weight = __builtin_bswap16(rawBuffer[i].weight);
    }

    s_loaded = true;
    return true;
}

bool isLoaded() {
    return s_loaded && !s_bookEntries.empty();
}

void close() {
    s_bookEntries.clear();
    s_bookEntries.shrink_to_fit();
    s_loaded = false;
}

uint64_t computePolyglotHash(const Board& board) {
    uint64_t hash = 0ULL;

    // 1. Pieces on squares
    for (int sq = 0; sq < 64; ++sq) {
        int piece = board.pieceOnSq[sq];
        if (piece != NO_PIECE) {
            int polyPiece = pieceType(piece) * 2 + (pieceColor(piece) == WHITE ? 1 : 0);
            hash ^= PolyglotRandom[64 * polyPiece + sq];
        }
    }

    // 2. Castling rights
    if (board.castlingRights & WHITE_OO)  hash ^= PolyglotRandom[768];
    if (board.castlingRights & WHITE_OOO) hash ^= PolyglotRandom[769];
    if (board.castlingRights & BLACK_OO)  hash ^= PolyglotRandom[770];
    if (board.castlingRights & BLACK_OOO) hash ^= PolyglotRandom[771];

    // 3. En passant square
    // In PolyGlot, EP is ONLY hashed if an active pawn can legally/actually capture it!
    if (board.epSquare != SQ_NONE) {
        int ep = board.epSquare;
        int epFile = squareFile(ep);
        bool canCapture = false;

        if (board.sideToMove == WHITE) {
            // White captures to rank 6 (indices 40-47). Capturing pawns must be on rank 5.
            if (epFile > 0 && board.pieceOnSq[ep - 9] == W_PAWN) canCapture = true;
            if (epFile < 7 && board.pieceOnSq[ep - 7] == W_PAWN) canCapture = true;
        } else {
            // Black captures to rank 3 (indices 16-23). Capturing pawns must be on rank 4.
            if (epFile > 0 && board.pieceOnSq[ep + 7] == B_PAWN) canCapture = true;
            if (epFile < 7 && board.pieceOnSq[ep + 9] == B_PAWN) canCapture = true;
        }

        if (canCapture) {
            hash ^= PolyglotRandom[772 + epFile];
        }
    }

    // 4. Turn (White to move)
    if (board.sideToMove == WHITE) {
        hash ^= PolyglotRandom[780];
    }

    return hash;
}

Move getBookMove(Board& board, int maxMoves) {
    if (!s_loaded || s_bookEntries.empty()) {
        return Move();
    }

    // Respect max opening moves threshold
    if (board.fullmoveNumber > maxMoves) {
        return Move();
    }

    uint64_t hash = computePolyglotHash(board);

    // Binary search for hash in sorted book entries
    auto it = std::lower_bound(
        s_bookEntries.begin(),
        s_bookEntries.end(),
        hash,
        [](const BookEntry& entry, uint64_t key) {
            return entry.key < key;
        }
    );

    if (it == s_bookEntries.end() || it->key != hash) {
        return Move(); // Book miss
    }

    // Collect all candidate moves for this position
    struct Candidate {
        uint16_t rawMove;
        uint16_t weight;
    };
    std::vector<Candidate> candidates;
    uint32_t totalWeight = 0;

    for (auto cur = it; cur != s_bookEntries.end() && cur->key == hash; ++cur) {
        if (cur->weight > 0) {
            candidates.push_back({cur->move, cur->weight});
            totalWeight += cur->weight;
        }
    }

    if (candidates.empty()) {
        return Move();
    }

    // Weighted random selection
    std::uniform_int_distribution<uint32_t> dist(0, totalWeight - 1);
    uint32_t pick = dist(s_rng);
    uint32_t sum = 0;
    uint16_t selectedRaw = candidates[0].rawMove;

    for (const auto& c : candidates) {
        sum += c.weight;
        if (sum > pick) {
            selectedRaw = c.rawMove;
            break;
        }
    }

    // Decode PolyGlot move
    int toSq = selectedRaw & 0x3F;
    int fromSq = (selectedRaw >> 6) & 0x3F;
    int promCode = (selectedRaw >> 12) & 0x7;

    int piece = board.pieceOnSq[fromSq];

    // Handle PolyGlot castling conversion (King takes Rook notation)
    if (pieceType(piece) == KING) {
        if (fromSq == E1 && toSq == H1) toSq = G1;
        else if (fromSq == E1 && toSq == A1) toSq = C1;
        else if (fromSq == E8 && toSq == H8) toSq = G8;
        else if (fromSq == E8 && toSq == A8) toSq = C8;
    }

    // Map promotion code
    int targetPromType = NONE;
    if (promCode == 1) targetPromType = KNIGHT;
    else if (promCode == 2) targetPromType = BISHOP;
    else if (promCode == 3) targetPromType = ROOK;
    else if (promCode == 4) targetPromType = QUEEN;

    // Validate against Voraxis legal moves
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
    for (const auto& m : legalMoves) {
        if (m.from == fromSq && m.to == toSq) {
            if (promCode == 0 || pieceType(m.promotion) == targetPromType) {
                return m;
            }
        }
    }

    return Move();
}

} // namespace Book
} // namespace Voraxis
