#include "see.h"
#include "movegen.h"
#include <algorithm>

namespace Voraxis {
namespace SEE {

static constexpr int pieceValues[7] = {
    100, 300, 320, 500, 900, 20000, 0
};

static uint64_t getAttackers(const Board& board, int targetSq, uint64_t occ) {
    uint64_t attackers = 0ULL;

    attackers |= (Movegen::getPawnAttacks(BLACK, targetSq) & board.pieceBB[W_PAWN]);
    attackers |= (Movegen::getPawnAttacks(WHITE, targetSq) & board.pieceBB[B_PAWN]);

    attackers |= (Movegen::getKnightAttacks(targetSq) & (board.pieceBB[W_KNIGHT] | board.pieceBB[B_KNIGHT]));

    attackers |= (Movegen::getKingAttacks(targetSq) & (board.pieceBB[W_KING] | board.pieceBB[B_KING]));

    uint64_t diagPieces = board.pieceBB[W_BISHOP] | board.pieceBB[B_BISHOP] | board.pieceBB[W_QUEEN] | board.pieceBB[B_QUEEN];
    attackers |= (Movegen::getBishopAttacks(targetSq, occ) & diagPieces);

    uint64_t straightPieces = board.pieceBB[W_ROOK] | board.pieceBB[B_ROOK] | board.pieceBB[W_QUEEN] | board.pieceBB[B_QUEEN];
    attackers |= (Movegen::getRookAttacks(targetSq, occ) & straightPieces);

    return attackers & occ;
}

static bool getLeastValuableAttacker(const Board& board, int targetSq, int side, uint64_t occ,
                                     uint64_t attackers, int& attackerSq, int& attackerType) {

    int pawnPiece = (side == WHITE) ? W_PAWN : B_PAWN;
    uint64_t subset = attackers & board.pieceBB[pawnPiece] & occ;
    if (subset) {
        attackerSq = __builtin_ctzll(subset);
        attackerType = PAWN;
        return true;
    }

    int knightPiece = (side == WHITE) ? W_KNIGHT : B_KNIGHT;
    subset = attackers & board.pieceBB[knightPiece] & occ;
    if (subset) {
        attackerSq = __builtin_ctzll(subset);
        attackerType = KNIGHT;
        return true;
    }

    int bishopPiece = (side == WHITE) ? W_BISHOP : B_BISHOP;
    subset = attackers & board.pieceBB[bishopPiece] & occ;
    if (subset) {
        attackerSq = __builtin_ctzll(subset);
        attackerType = BISHOP;
        return true;
    }

    int rookPiece = (side == WHITE) ? W_ROOK : B_ROOK;
    subset = attackers & board.pieceBB[rookPiece] & occ;
    if (subset) {
        attackerSq = __builtin_ctzll(subset);
        attackerType = ROOK;
        return true;
    }

    int queenPiece = (side == WHITE) ? W_QUEEN : B_QUEEN;
    subset = attackers & board.pieceBB[queenPiece] & occ;
    if (subset) {
        attackerSq = __builtin_ctzll(subset);
        attackerType = QUEEN;
        return true;
    }

    int kingPiece = (side == WHITE) ? W_KING : B_KING;
    subset = attackers & board.pieceBB[kingPiece] & occ;
    if (subset) {
        uint64_t enemyOcc = (side == WHITE) ? board.occBB[BLACK] : board.occBB[WHITE];
        if ((attackers & enemyOcc & occ) == 0) {
            attackerSq = __builtin_ctzll(subset);
            attackerType = KING;
            return true;
        }
    }

    return false;
}

int evaluate(const Board& board, const Move& move) {
    int fromSq = move.from;
    int toSq = move.to;

    if (move.isCastling) return 0;

    int capturedPiece = move.captured;
    int capturedVal = (capturedPiece != NO_PIECE) ? pieceValues[pieceType(capturedPiece)] : 0;

    if (move.isEnPassant) {
        capturedVal = pieceValues[PAWN];
    }

    int gain[32];
    int d = 0;
    gain[d] = capturedVal;

    if (move.promotion != NO_PIECE) {
        gain[d] += pieceValues[pieceType(move.promotion)] - pieceValues[PAWN];
    }

    uint64_t occ = board.occBB[WHITE] | board.occBB[BLACK];
    occ &= ~(1ULL << fromSq);

    if (move.isEnPassant) {
        int epCapSq = makeSquare(squareFile(toSq), squareRank(fromSq));
        occ &= ~(1ULL << epCapSq);
    }

    uint64_t attackers = getAttackers(board, toSq, occ);

    int currentPieceVal = (move.promotion != NO_PIECE)
        ? pieceValues[pieceType(move.promotion)]
        : pieceValues[pieceType(move.piece)];

    int side = pieceColor(move.piece) ^ 1;

    while (d < 31) {
        d++;

        gain[d] = currentPieceVal - gain[d - 1];

        int attackerSq = SQ_NONE;
        int attackerType = NONE;

        if (!getLeastValuableAttacker(board, toSq, side, occ, attackers, attackerSq, attackerType)) {
            break;
        }

        currentPieceVal = pieceValues[attackerType];

        occ &= ~(1ULL << attackerSq);

        if (attackerType == PAWN || attackerType == BISHOP || attackerType == QUEEN) {
            uint64_t diagPieces = board.pieceBB[W_BISHOP] | board.pieceBB[B_BISHOP] | board.pieceBB[W_QUEEN] | board.pieceBB[B_QUEEN];
            attackers |= (Movegen::getBishopAttacks(toSq, occ) & diagPieces);
        }
        if (attackerType == ROOK || attackerType == QUEEN) {
            uint64_t straightPieces = board.pieceBB[W_ROOK] | board.pieceBB[B_ROOK] | board.pieceBB[W_QUEEN] | board.pieceBB[B_QUEEN];
            attackers |= (Movegen::getRookAttacks(toSq, occ) & straightPieces);
        }
        attackers &= occ;

        side ^= 1;
    }

    while (--d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    }

    return gain[0];
}

bool hasPositiveSEE(const Board& board, const Move& move, int threshold) {
    return evaluate(board, move) >= threshold;
}

}
}
