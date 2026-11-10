#include "movegen.h"
#include "magic.h"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace Voraxis {
namespace Movegen {

static uint64_t pawnAttacks[2][64];
static uint64_t knightAttacks[64];
static uint64_t kingAttacks[64];
static bool tablesInitialized = false;

void initAttackTables() {
    if (tablesInitialized) return;

    for (int sq = 0; sq < 64; ++sq) {
        int f = squareFile(sq);
        int r = squareRank(sq);

        // White pawn attacks
        pawnAttacks[WHITE][sq] = 0ULL;
        if (r < 7) {
            if (f > 0) pawnAttacks[WHITE][sq] |= (1ULL << makeSquare(f - 1, r + 1));
            if (f < 7) pawnAttacks[WHITE][sq] |= (1ULL << makeSquare(f + 1, r + 1));
        }

        // Black pawn attacks
        pawnAttacks[BLACK][sq] = 0ULL;
        if (r > 0) {
            if (f > 0) pawnAttacks[BLACK][sq] |= (1ULL << makeSquare(f - 1, r - 1));
            if (f < 7) pawnAttacks[BLACK][sq] |= (1ULL << makeSquare(f + 1, r - 1));
        }

        // Knight attacks
        knightAttacks[sq] = 0ULL;
        const int kdf[8] = {-2, -2, -1, -1,  1,  1,  2,  2};
        const int kdr[8] = {-1,  1, -2,  2, -2,  2, -1,  1};
        for (int i = 0; i < 8; ++i) {
            int nf = f + kdf[i];
            int nr = r + kdr[i];
            if (nf >= 0 && nf <= 7 && nr >= 0 && nr <= 7) {
                knightAttacks[sq] |= (1ULL << makeSquare(nf, nr));
            }
        }

        // King attacks
        kingAttacks[sq] = 0ULL;
        const int dirF[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
        const int dirR[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
        for (int i = 0; i < 8; ++i) {
            int kf = f + dirF[i];
            int kr = r + dirR[i];
            if (kf >= 0 && kf <= 7 && kr >= 0 && kr <= 7) {
                kingAttacks[sq] |= (1ULL << makeSquare(kf, kr));
            }
        }
    }

    Magic::initMagicTables();
    tablesInitialized = true;
}

uint64_t getPawnAttacks(int color, int sq) {
    if (!tablesInitialized) initAttackTables();
    return pawnAttacks[color][sq];
}

uint64_t getKnightAttacks(int sq) {
    if (!tablesInitialized) initAttackTables();
    return knightAttacks[sq];
}

uint64_t getKingAttacks(int sq) {
    if (!tablesInitialized) initAttackTables();
    return kingAttacks[sq];
}

uint64_t getBishopAttacks(int sq, uint64_t occ) {
    return Magic::getBishopAttacks(sq, occ);
}

uint64_t getRookAttacks(int sq, uint64_t occ) {
    return Magic::getRookAttacks(sq, occ);
}

uint64_t getQueenAttacks(int sq, uint64_t occ) {
    return Magic::getQueenAttacks(sq, occ);
}

bool isSquareAttacked(const Board& board, int sq, int attackerColor) {
    if (attackerColor == WHITE) {
        if (pawnAttacks[BLACK][sq] & board.pieceBB[W_PAWN]) return true;
        if (knightAttacks[sq] & board.pieceBB[W_KNIGHT]) return true;
        if (kingAttacks[sq] & board.pieceBB[W_KING]) return true;
        if (getBishopAttacks(sq, board.occBB[BOTH]) & (board.pieceBB[W_BISHOP] | board.pieceBB[W_QUEEN])) return true;
        if (getRookAttacks(sq, board.occBB[BOTH]) & (board.pieceBB[W_ROOK] | board.pieceBB[W_QUEEN])) return true;
    } else {
        if (pawnAttacks[WHITE][sq] & board.pieceBB[B_PAWN]) return true;
        if (knightAttacks[sq] & board.pieceBB[B_KNIGHT]) return true;
        if (kingAttacks[sq] & board.pieceBB[B_KING]) return true;
        if (getBishopAttacks(sq, board.occBB[BOTH]) & (board.pieceBB[B_BISHOP] | board.pieceBB[B_QUEEN])) return true;
        if (getRookAttacks(sq, board.occBB[BOTH]) & (board.pieceBB[B_ROOK] | board.pieceBB[B_QUEEN])) return true;
    }
    return false;
}

bool inCheck(const Board& board, int color) {
    int kingSq = board.getKingSquare(color);
    if (kingSq == SQ_NONE) return false;
    return isSquareAttacked(board, kingSq, color ^ 1);
}

std::vector<Move> generateLegalMoves(Board& board, MoveGenType type) {
    initAttackTables();
    std::vector<Move> pseudoMoves;
    pseudoMoves.reserve(type == GEN_CAPTURES ? 16 : 64);

    int side = board.sideToMove;
    int enemyColor = side ^ 1;
    bool genCaptures = (type == GEN_ALL || type == GEN_CAPTURES);
    bool genQuiets = (type == GEN_ALL || type == GEN_QUIETS);

    uint64_t targetMask = 0ULL;
    if (genCaptures && genQuiets) {
        targetMask = ~board.occBB[side];
    } else if (genCaptures) {
        targetMask = board.occBB[enemyColor];
    } else {
        targetMask = ~board.occBB[BOTH];
    }

    // 1. Pawn Moves
    if (side == WHITE) {
        uint64_t pawns = board.pieceBB[W_PAWN];
        while (pawns) {
            int sq = __builtin_ctzll(pawns);
            pawns &= pawns - 1;

            int f = squareFile(sq);
            int r = squareRank(sq);

            // Single push & Promotions
            int singleSq = sq + 8;
            if (board.pieceOnSq[singleSq] == NO_PIECE) {
                if (r == 6) { // Promotion is tactical (included in captures)
                    if (genCaptures) {
                        pseudoMoves.emplace_back(sq, singleSq, W_PAWN, NO_PIECE, W_QUEEN);
                        pseudoMoves.emplace_back(sq, singleSq, W_PAWN, NO_PIECE, W_ROOK);
                        pseudoMoves.emplace_back(sq, singleSq, W_PAWN, NO_PIECE, W_BISHOP);
                        pseudoMoves.emplace_back(sq, singleSq, W_PAWN, NO_PIECE, W_KNIGHT);
                    }
                } else if (genQuiets) {
                    pseudoMoves.emplace_back(sq, singleSq, W_PAWN);
                    // Double push
                    if (r == 1) {
                        int doubleSq = sq + 16;
                        if (board.pieceOnSq[doubleSq] == NO_PIECE) {
                            pseudoMoves.emplace_back(sq, doubleSq, W_PAWN, NO_PIECE, NO_PIECE, false, false, true);
                        }
                    }
                }
            }

            // Captures
            if (genCaptures) {
                if (f > 0) {
                    int capSq = sq + 7;
                    int capPiece = board.pieceOnSq[capSq];
                    if (capPiece != NO_PIECE && pieceColor(capPiece) == enemyColor) {
                        if (r == 6) {
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_QUEEN);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_ROOK);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_BISHOP);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_KNIGHT);
                        } else {
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece);
                        }
                    } else if (capSq == board.epSquare) {
                        pseudoMoves.emplace_back(sq, capSq, W_PAWN, B_PAWN, NO_PIECE, false, true, false);
                    }
                }

                if (f < 7) {
                    int capSq = sq + 9;
                    int capPiece = board.pieceOnSq[capSq];
                    if (capPiece != NO_PIECE && pieceColor(capPiece) == enemyColor) {
                        if (r == 6) {
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_QUEEN);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_ROOK);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_BISHOP);
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece, W_KNIGHT);
                        } else {
                            pseudoMoves.emplace_back(sq, capSq, W_PAWN, capPiece);
                        }
                    } else if (capSq == board.epSquare) {
                        pseudoMoves.emplace_back(sq, capSq, W_PAWN, B_PAWN, NO_PIECE, false, true, false);
                    }
                }
            }
        }
    } else {
        uint64_t pawns = board.pieceBB[B_PAWN];
        while (pawns) {
            int sq = __builtin_ctzll(pawns);
            pawns &= pawns - 1;

            int f = squareFile(sq);
            int r = squareRank(sq);

            // Single push & Promotions
            int singleSq = sq - 8;
            if (board.pieceOnSq[singleSq] == NO_PIECE) {
                if (r == 1) { // Promotion is tactical (included in captures)
                    if (genCaptures) {
                        pseudoMoves.emplace_back(sq, singleSq, B_PAWN, NO_PIECE, B_QUEEN);
                        pseudoMoves.emplace_back(sq, singleSq, B_PAWN, NO_PIECE, B_ROOK);
                        pseudoMoves.emplace_back(sq, singleSq, B_PAWN, NO_PIECE, B_BISHOP);
                        pseudoMoves.emplace_back(sq, singleSq, B_PAWN, NO_PIECE, B_KNIGHT);
                    }
                } else if (genQuiets) {
                    pseudoMoves.emplace_back(sq, singleSq, B_PAWN);
                    // Double push
                    if (r == 6) {
                        int doubleSq = sq - 16;
                        if (board.pieceOnSq[doubleSq] == NO_PIECE) {
                            pseudoMoves.emplace_back(sq, doubleSq, B_PAWN, NO_PIECE, NO_PIECE, false, false, true);
                        }
                    }
                }
            }

            // Captures
            if (genCaptures) {
                if (f > 0) {
                    int capSq = sq - 9;
                    int capPiece = board.pieceOnSq[capSq];
                    if (capPiece != NO_PIECE && pieceColor(capPiece) == enemyColor) {
                        if (r == 1) {
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_QUEEN);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_ROOK);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_BISHOP);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_KNIGHT);
                        } else {
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece);
                        }
                    } else if (capSq == board.epSquare) {
                        pseudoMoves.emplace_back(sq, capSq, B_PAWN, W_PAWN, NO_PIECE, false, true, false);
                    }
                }

                if (f < 7) {
                    int capSq = sq - 7;
                    int capPiece = board.pieceOnSq[capSq];
                    if (capPiece != NO_PIECE && pieceColor(capPiece) == enemyColor) {
                        if (r == 1) {
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_QUEEN);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_ROOK);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_BISHOP);
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece, B_KNIGHT);
                        } else {
                            pseudoMoves.emplace_back(sq, capSq, B_PAWN, capPiece);
                        }
                    } else if (capSq == board.epSquare) {
                        pseudoMoves.emplace_back(sq, capSq, B_PAWN, W_PAWN, NO_PIECE, false, true, false);
                    }
                }
            }
        }
    }

    // 2. Knights
    int knightPiece = (side == WHITE) ? W_KNIGHT : B_KNIGHT;
    uint64_t knights = board.pieceBB[knightPiece];
    while (knights) {
        int sq = __builtin_ctzll(knights);
        knights &= knights - 1;
        uint64_t targets = knightAttacks[sq] & targetMask;
        while (targets) {
            int toSq = __builtin_ctzll(targets);
            targets &= targets - 1;
            int cap = board.pieceOnSq[toSq];
            pseudoMoves.emplace_back(sq, toSq, knightPiece, cap);
        }
    }

    // 3. Bishops
    int bishopPiece = (side == WHITE) ? W_BISHOP : B_BISHOP;
    uint64_t bishops = board.pieceBB[bishopPiece];
    while (bishops) {
        int sq = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        uint64_t targets = getBishopAttacks(sq, board.occBB[BOTH]) & targetMask;
        while (targets) {
            int toSq = __builtin_ctzll(targets);
            targets &= targets - 1;
            int cap = board.pieceOnSq[toSq];
            pseudoMoves.emplace_back(sq, toSq, bishopPiece, cap);
        }
    }

    // 4. Rooks
    int rookPiece = (side == WHITE) ? W_ROOK : B_ROOK;
    uint64_t rooks = board.pieceBB[rookPiece];
    while (rooks) {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        uint64_t targets = getRookAttacks(sq, board.occBB[BOTH]) & targetMask;
        while (targets) {
            int toSq = __builtin_ctzll(targets);
            targets &= targets - 1;
            int cap = board.pieceOnSq[toSq];
            pseudoMoves.emplace_back(sq, toSq, rookPiece, cap);
        }
    }

    // 5. Queens
    int queenPiece = (side == WHITE) ? W_QUEEN : B_QUEEN;
    uint64_t queens = board.pieceBB[queenPiece];
    while (queens) {
        int sq = __builtin_ctzll(queens);
        queens &= queens - 1;
        uint64_t targets = getQueenAttacks(sq, board.occBB[BOTH]) & targetMask;
        while (targets) {
            int toSq = __builtin_ctzll(targets);
            targets &= targets - 1;
            int cap = board.pieceOnSq[toSq];
            pseudoMoves.emplace_back(sq, toSq, queenPiece, cap);
        }
    }

    // 6. King & Castling
    int kingPiece = (side == WHITE) ? W_KING : B_KING;
    int kingSq = board.getKingSquare(side);
    if (kingSq != SQ_NONE) {
        uint64_t targets = kingAttacks[kingSq] & targetMask;
        while (targets) {
            int toSq = __builtin_ctzll(targets);
            targets &= targets - 1;
            int cap = board.pieceOnSq[toSq];
            pseudoMoves.emplace_back(kingSq, toSq, kingPiece, cap);
        }

        // Castling moves (quiet only)
        if (genQuiets && !inCheck(board, side)) {
            if (side == WHITE) {
                // White Kingside: e1 -> g1
                if ((board.castlingRights & WHITE_OO) &&
                    board.pieceOnSq[F1] == NO_PIECE && board.pieceOnSq[G1] == NO_PIECE &&
                    board.pieceOnSq[H1] == W_ROOK &&
                    !isSquareAttacked(board, F1, BLACK) && !isSquareAttacked(board, G1, BLACK)) {
                    pseudoMoves.emplace_back(E1, G1, W_KING, NO_PIECE, NO_PIECE, true);
                }
                // White Queenside: e1 -> c1
                if ((board.castlingRights & WHITE_OOO) &&
                    board.pieceOnSq[D1] == NO_PIECE && board.pieceOnSq[C1] == NO_PIECE && board.pieceOnSq[B1] == NO_PIECE &&
                    board.pieceOnSq[A1] == W_ROOK &&
                    !isSquareAttacked(board, D1, BLACK) && !isSquareAttacked(board, C1, BLACK)) {
                    pseudoMoves.emplace_back(E1, C1, W_KING, NO_PIECE, NO_PIECE, true);
                }
            } else {
                // Black Kingside: e8 -> g8
                if ((board.castlingRights & BLACK_OO) &&
                    board.pieceOnSq[F8] == NO_PIECE && board.pieceOnSq[G8] == NO_PIECE &&
                    board.pieceOnSq[H8] == B_ROOK &&
                    !isSquareAttacked(board, F8, WHITE) && !isSquareAttacked(board, G8, WHITE)) {
                    pseudoMoves.emplace_back(E8, G8, B_KING, NO_PIECE, NO_PIECE, true);
                }
                // Black Queenside: e8 -> c8
                if ((board.castlingRights & BLACK_OOO) &&
                    board.pieceOnSq[D8] == NO_PIECE && board.pieceOnSq[C8] == NO_PIECE && board.pieceOnSq[B8] == NO_PIECE &&
                    board.pieceOnSq[A8] == B_ROOK &&
                    !isSquareAttacked(board, D8, WHITE) && !isSquareAttacked(board, C8, WHITE)) {
                    pseudoMoves.emplace_back(E8, C8, B_KING, NO_PIECE, NO_PIECE, true);
                }
            }
        }
    }

    // Filter strictly legal moves
    std::vector<Move> legalMoves;
    legalMoves.reserve(pseudoMoves.size());
    for (const auto& m : pseudoMoves) {
        board.applyMove(m);
        if (!inCheck(board, side)) {
            legalMoves.push_back(m);
        }
        board.undoMove(m);
    }

    return legalMoves;
}

bool isCheckmate(Board& board) {
    if (!inCheck(board, board.getSideToMove())) return false;
    return generateLegalMoves(board).empty();
}

bool isStalemate(Board& board) {
    if (inCheck(board, board.getSideToMove())) return false;
    return generateLegalMoves(board).empty();
}

bool isDraw(Board& board) {
    return isStalemate(board) ||
           board.isFiftyMoveDraw() ||
           board.isThreefoldRepetition() ||
           board.isInsufficientMaterial();
}

uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1ULL;

    std::vector<Move> moves = generateLegalMoves(board);
    if (depth == 1) return moves.size();

    uint64_t nodes = 0;
    for (const auto& m : moves) {
        board.applyMove(m);
        nodes += perft(board, depth - 1);
        board.undoMove(m);
    }
    return nodes;
}

struct PerftTestCase {
    const char* name;
    const char* fen;
    int depth;
    uint64_t expectedNodes;
};

bool runPerftSuite() {
    initAttackTables();
    Board::initZobrist();

    std::vector<PerftTestCase> suite = {
        // Position 1: Startpos
        {"Startpos Depth 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20ULL},
        {"Startpos Depth 2", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400ULL},
        {"Startpos Depth 3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ULL},
        {"Startpos Depth 4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL},
        {"Startpos Depth 5", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL},

        // Position 2: Kiwipete
        {"Kiwipete Depth 1", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48ULL},
        {"Kiwipete Depth 2", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039ULL},
        {"Kiwipete Depth 3", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL},
        {"Kiwipete Depth 4", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL},

        // Position 3
        {"Position 3 Depth 1", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14ULL},
        {"Position 3 Depth 2", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191ULL},
        {"Position 3 Depth 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL},
        {"Position 3 Depth 4", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238ULL},
        {"Position 3 Depth 5", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL},

        // Position 4
        {"Position 4 Depth 1", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6ULL},
        {"Position 4 Depth 2", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264ULL},
        {"Position 4 Depth 3", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467ULL},
        {"Position 4 Depth 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333ULL},

        // Position 5
        {"Position 5 Depth 1", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 1, 44ULL},
        {"Position 5 Depth 2", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 2, 1486ULL},
        {"Position 5 Depth 3", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL},
        {"Position 5 Depth 4", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL}
    };

    std::cout << "========================================================\n";
    std::cout << "          VORAXIS CHESS ENGINE - PERFT TEST SUITE       \n";
    std::cout << "========================================================\n";

    bool allPassed = true;
    uint64_t totalNodes = 0;
    auto overallStart = std::chrono::high_resolution_clock::now();

    Board board;
    for (const auto& test : suite) {
        board.loadFEN(test.fen);
        auto tStart = std::chrono::high_resolution_clock::now();
        uint64_t nodes = perft(board, test.depth);
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        totalNodes += nodes;
        bool pass = (nodes == test.expectedNodes);
        if (!pass) allPassed = false;

        std::cout << std::left << std::setw(22) << test.name
                  << " Nodes: " << std::setw(10) << nodes
                  << " Expected: " << std::setw(10) << test.expectedNodes
                  << " Time: " << std::setw(8) << std::fixed << std::setprecision(1) << ms << " ms "
                  << (pass ? "[ PASS ]" : "[ FAIL ]") << "\n";
    }

    auto overallEnd = std::chrono::high_resolution_clock::now();
    double totalSeconds = std::chrono::duration<double>(overallEnd - overallStart).count();
    double nps = (totalSeconds > 0.0) ? (totalNodes / totalSeconds) : 0.0;

    std::cout << "--------------------------------------------------------\n";
    std::cout << "Total Nodes: " << totalNodes << " in " << std::fixed << std::setprecision(3)
              << totalSeconds << " s (" << static_cast<uint64_t>(nps) << " nodes/sec)\n";
    std::cout << "Test Suite Result: " << (allPassed ? "ALL TESTS PASSED! PERFECT MOVE GENERATION." : "SOME TESTS FAILED!") << "\n";
    std::cout << "========================================================\n";

    return allPassed;
}

} // namespace Movegen
} // namespace Voraxis
