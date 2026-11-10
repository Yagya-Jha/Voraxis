#include "syzygy_bridge.h"
#include "syzygy/tbprobe.h"
#include "movegen.h"
#include "search.h"
#include <iostream>

namespace Voraxis {
namespace Syzygy {

static std::string loadedPath = "";

bool init(const std::string& path) {
    if (path.empty()) {
        tb_free();
        loadedPath = "";
        return false;
    }
    bool ok = tb_init(path.c_str());
    if (ok && TB_LARGEST > 0) {
        loadedPath = path;
        std::cout << "info string Syzygy tablebases initialized: max " << TB_LARGEST << " pieces from " << path << std::endl;
        return true;
    } else {
        loadedPath = "";
        return false;
    }
}

void free() {
    tb_free();
    loadedPath = "";
}

bool isLoaded() {
    return TB_LARGEST > 0;
}

int getMaxPieces() {
    return TB_LARGEST;
}

WDLResult probeWDL(const Board& board) {
    if (TB_LARGEST == 0) return WDL_FAILED;
    if (board.castlingRights != NO_CASTLING) return WDL_FAILED;

    uint64_t occAll = board.occBB[WHITE] | board.occBB[BLACK];
    int pieceCount = __builtin_popcountll(occAll);
    if (pieceCount > (int)TB_LARGEST) return WDL_FAILED;

    uint64_t white = board.occBB[WHITE];
    uint64_t black = board.occBB[BLACK];
    uint64_t kings   = board.pieceBB[W_KING]   | board.pieceBB[B_KING];
    uint64_t queens  = board.pieceBB[W_QUEEN]  | board.pieceBB[B_QUEEN];
    uint64_t rooks   = board.pieceBB[W_ROOK]   | board.pieceBB[B_ROOK];
    uint64_t bishops = board.pieceBB[W_BISHOP] | board.pieceBB[B_BISHOP];
    uint64_t knights = board.pieceBB[W_KNIGHT] | board.pieceBB[B_KNIGHT];
    uint64_t pawns   = board.pieceBB[W_PAWN]   | board.pieceBB[B_PAWN];
    unsigned ep = (board.epSquare != SQ_NONE) ? board.epSquare : 0;
    bool turn = (board.sideToMove == WHITE);

    unsigned res = tb_probe_wdl_impl(white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn);
    if (res == TB_RESULT_FAILED) {
        return WDL_FAILED;
    }
    return static_cast<WDLResult>(res);
}

bool probeRoot(const Board& board, Move& bestMove, int& wdlScore) {
    if (TB_LARGEST == 0) return false;
    if (board.castlingRights != NO_CASTLING) return false;

    uint64_t occAll = board.occBB[WHITE] | board.occBB[BLACK];
    int pieceCount = __builtin_popcountll(occAll);
    if (pieceCount > (int)TB_LARGEST) return false;

    uint64_t white = board.occBB[WHITE];
    uint64_t black = board.occBB[BLACK];
    uint64_t kings   = board.pieceBB[W_KING]   | board.pieceBB[B_KING];
    uint64_t queens  = board.pieceBB[W_QUEEN]  | board.pieceBB[B_QUEEN];
    uint64_t rooks   = board.pieceBB[W_ROOK]   | board.pieceBB[B_ROOK];
    uint64_t bishops = board.pieceBB[W_BISHOP] | board.pieceBB[B_BISHOP];
    uint64_t knights = board.pieceBB[W_KNIGHT] | board.pieceBB[B_KNIGHT];
    uint64_t pawns   = board.pieceBB[W_PAWN]   | board.pieceBB[B_PAWN];
    unsigned rule50 = board.halfmoveClock;
    unsigned ep = (board.epSquare != SQ_NONE) ? board.epSquare : 0;
    bool turn = (board.sideToMove == WHITE);

    unsigned results[TB_MAX_MOVES];
    unsigned res = tb_probe_root_impl(white, black, kings, queens, rooks, bishops, knights, pawns, rule50, ep, turn, results);
    if (res == TB_RESULT_FAILED || res == TB_RESULT_CHECKMATE || res == TB_RESULT_STALEMATE) {
        return false;
    }

    int from = TB_GET_FROM(res);
    int to = TB_GET_TO(res);
    int prom = TB_GET_PROMOTES(res);
    unsigned wdl = TB_GET_WDL(res);

    int expectedPromType = NONE;
    if (prom == TB_PROMOTES_QUEEN) expectedPromType = QUEEN;
    else if (prom == TB_PROMOTES_ROOK) expectedPromType = ROOK;
    else if (prom == TB_PROMOTES_BISHOP) expectedPromType = BISHOP;
    else if (prom == TB_PROMOTES_KNIGHT) expectedPromType = KNIGHT;

    // Match with Voraxis legal move list to ensure all Move fields are exact
    Board copyBoard = board;
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(copyBoard);
    for (const auto& m : legalMoves) {
        if (m.from == from && m.to == to) {
            if (expectedPromType == NONE && m.promotion == NO_PIECE) {
                bestMove = m;
                break;
            }
            if (expectedPromType != NONE && m.promotion != NO_PIECE && pieceType(m.promotion) == expectedPromType) {
                bestMove = m;
                break;
            }
        }
    }

    if (bestMove.from == SQ_NONE) {
        return false;
    }

    if (wdl == TB_WIN) {
        wdlScore = Search::MATE_THRESHOLD - 1000;
    } else if (wdl == TB_LOSS) {
        wdlScore = -(Search::MATE_THRESHOLD - 1000);
    } else if (wdl == TB_CURSED_WIN) {
        wdlScore = 1;
    } else if (wdl == TB_BLESSED_LOSS) {
        wdlScore = -1;
    } else {
        wdlScore = 0;
    }

    return true;
}

} // namespace Syzygy
} // namespace Voraxis
