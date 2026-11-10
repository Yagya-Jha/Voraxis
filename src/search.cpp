#include "search.h"
#include "threadpool.h"
#include "board.h"
#include "evaluate.h"
#include "syzygy_bridge.h"
#include "see.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace Voraxis {
namespace Search {

std::atomic<bool> stopSearch(false);
size_t ttMask = TT_ENTRIES - 1;
std::vector<TTEntry> ttTable(TT_ENTRIES);

int scoreToTT(int score, int ply) {
    if (score >= MATE_THRESHOLD) return score + ply;
    if (score <= -MATE_THRESHOLD) return score - ply;
    return score;
}

int scoreFromTT(int score, int ply) {
    if (score >= MATE_THRESHOLD) return score - ply;
    if (score <= -MATE_THRESHOLD) return score + ply;
    return score;
}

TTEntry* probeTT(uint64_t key) {
    size_t index = key & ttMask;
    uint64_t xorKey = ttTable[index].key;
    uint64_t expected = key ^ ((uint64_t)(uint32_t)ttTable[index].score << 32) ^ ((uint64_t)(uint16_t)ttTable[index].depth << 16) ^ (uint64_t)ttTable[index].flag;
    if (xorKey == expected) {
        return &ttTable[index];
    }
    return nullptr;
}

void storeTT(uint64_t key, int depth, int score, TTFlag flag, Move bestMove, int ply) {
    size_t index = key & ttMask;
    TTEntry& entry = ttTable[index];

    uint64_t xorKey = entry.key;
    uint64_t expected = key ^ ((uint64_t)(uint32_t)entry.score << 32) ^ ((uint64_t)(uint16_t)entry.depth << 16) ^ (uint64_t)entry.flag;
    bool isSameKey = (xorKey == expected);

    if (entry.key == 0 || isSameKey || depth >= entry.depth) {
        int ttScore = scoreToTT(score, ply);
        entry.depth = depth;
        entry.score = ttScore;
        entry.flag = flag;
        if (bestMove.from != SQ_NONE) {
            entry.bestMove = bestMove;
        } else if (!isSameKey) {
            entry.bestMove = Move();
        }
        entry.key = key ^ ((uint64_t)(uint32_t)ttScore << 32) ^ ((uint64_t)(uint16_t)depth << 16) ^ (uint64_t)flag;
    }
}

void clearTT() {
    std::fill(ttTable.begin(), ttTable.end(), TTEntry{});
}

void setTTSize(size_t mbSize) {
    mbSize = std::max((size_t)1, std::min(mbSize, (size_t)2048));
    size_t targetEntries = (mbSize * 1024 * 1024) / sizeof(TTEntry);
    size_t entries = 1;
    while (entries * 2 <= targetEntries) {
        entries *= 2;
    }
    ttMask = entries - 1;
    ttTable.clear();
    ttTable.resize(entries, TTEntry{});
}

int getHashFull() {
    int sampleSize = std::min((size_t)1000, ttTable.size());
    if (sampleSize == 0) return 0;
    int used = 0;
    for (int i = 0; i < sampleSize; ++i) {
        if (ttTable[i].key != 0) used++;
    }
    return (used * 1000) / sampleSize;
}

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void checkTime(const SearchLimits& limits, long long nodes) {
    if ((nodes & 2047) == 0) {
        if (limits.time > 0 && nowMs() - limits.startTime >= limits.time) {
            stopSearch.store(true, std::memory_order_relaxed);
            return;
        }
#ifdef _WIN32
        static HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(hStdin, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
            std::string line;
            if (std::getline(std::cin, line)) {
                if (line == "stop") {
                    stopSearch.store(true, std::memory_order_relaxed);
                } else if (line == "quit") {
                    stopSearch.store(true, std::memory_order_relaxed);
                    exit(0);
                }
            }
        }
#endif
    }
}

int getPieceValue(int pType) {
    constexpr int arr[] = {100, 300, 350, 500, 900, 10000};
    return arr[pType];
}

bool isPseudoLegal(const Board& b, const Move& m) {
    if (m.from < 0 || m.from > 63 || m.to < 0 || m.to > 63) return false;
    int p = b.pieceOnSq[m.from];
    if (p == NO_PIECE || pieceColor(p) != b.getSideToMove()) return false;
    if (m.piece != p) return false;
    int cap = b.pieceOnSq[m.to];
    if (cap != NO_PIECE && pieceColor(cap) == b.getSideToMove()) return false;

    int pt = pieceType(p);
    int side = b.getSideToMove();
    uint64_t occ = b.occBB[BOTH];

    switch (pt) {
        case PAWN: {
            int dir = (side == WHITE) ? 8 : -8;
            int startRank = (side == WHITE) ? 1 : 6;
            int rFrom = squareRank(m.from);
            int rTo = squareRank(m.to);
            int fFrom = squareFile(m.from);
            int fTo = squareFile(m.to);
            if (fFrom == fTo) {
                if (m.to == m.from + dir && b.pieceOnSq[m.to] == NO_PIECE) return true;
                if (rFrom == startRank && m.to == m.from + 2 * dir &&
                    b.pieceOnSq[m.from + dir] == NO_PIECE && b.pieceOnSq[m.to] == NO_PIECE) return true;
            } else if (std::abs(fFrom - fTo) == 1 && rTo == rFrom + (side == WHITE ? 1 : -1)) {
                if (cap != NO_PIECE && pieceColor(cap) != side) return true;
                if (m.to == b.epSquare) return true;
            }
            return false;
        }
        case KNIGHT:
            return (Movegen::getKnightAttacks(m.from) & (1ULL << m.to)) != 0;
        case BISHOP:
            return (Movegen::getBishopAttacks(m.from, occ) & (1ULL << m.to)) != 0;
        case ROOK:
            return (Movegen::getRookAttacks(m.from, occ) & (1ULL << m.to)) != 0;
        case QUEEN:
            return (Movegen::getQueenAttacks(m.from, occ) & (1ULL << m.to)) != 0;
        case KING:
            if (Movegen::getKingAttacks(m.from) & (1ULL << m.to)) return true;
            if (m.isCastling) {
                if (side == WHITE) {
                    if (m.to == G1 && (b.castlingRights & WHITE_OO) && !(occ & ((1ULL << F1) | (1ULL << G1)))) {
                        return !Movegen::isSquareAttacked(b, E1, BLACK) && !Movegen::isSquareAttacked(b, F1, BLACK);
                    }
                    if (m.to == C1 && (b.castlingRights & WHITE_OOO) && !(occ & ((1ULL << D1) | (1ULL << C1) | (1ULL << B1)))) {
                        return !Movegen::isSquareAttacked(b, E1, BLACK) && !Movegen::isSquareAttacked(b, D1, BLACK);
                    }
                } else {
                    if (m.to == G8 && (b.castlingRights & BLACK_OO) && !(occ & ((1ULL << F8) | (1ULL << G8)))) {
                        return !Movegen::isSquareAttacked(b, E8, WHITE) && !Movegen::isSquareAttacked(b, F8, WHITE);
                    }
                    if (m.to == C8 && (b.castlingRights & BLACK_OOO) && !(occ & ((1ULL << D8) | (1ULL << C8) | (1ULL << B8)))) {
                        return !Movegen::isSquareAttacked(b, E8, WHITE) && !Movegen::isSquareAttacked(b, D8, WHITE);
                    }
                }
                return false;
            }
            return false;
        default:
            return false;
    }
}

bool isLegalMove(Board& b, const Move& m) {
    if (!isPseudoLegal(b, m)) return false;
    int side = b.getSideToMove();
    b.applyMove(m);
    bool legal = !Movegen::inCheck(b, side);
    b.undoMove(m);
    return legal;
}

std::vector<Move> extractPV(Board board, int depth) {
    std::vector<Move> pv;
    std::vector<uint64_t> seen;
    for (int i = 0; i < depth && i < 64; ++i) {
        TTEntry* entry = probeTT(board.zobristKey);
        if (!entry || entry->bestMove.from == SQ_NONE) {
            break;
        }
        Move m = entry->bestMove;
        if (!isLegalMove(board, m)) break;
        if (std::find(seen.begin(), seen.end(), board.zobristKey) != seen.end()) break;
        seen.push_back(board.zobristKey);
        pv.push_back(m);
        board.applyMove(m);
    }
    return pv;
}

// ----------------------------------------------------------------------------
// SearchWorker Implementation
// ----------------------------------------------------------------------------

SearchWorker::SearchWorker(int id) : threadId(id) {
    reset();
}

void SearchWorker::reset() {
    searchedNodes = 0;
    seldepth = 0;
    bestScore = 0;
    bestMove = Move();
    std::memset(killerMoves, 0, sizeof(killerMoves));
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(counterMoveTable, 0, sizeof(counterMoveTable));
}

int SearchWorker::scoreMove(const Move& move, int ply, const Move& ttMove, const Move& prevMove) {
    // 1. TT / Hash move
    if (ttMove.from != SQ_NONE && move == ttMove) {
        return 30000;
    }

    // Captures (Winning & equal captures vs Bad captures)
    if (move.captured != NO_PIECE || move.isEnPassant) {
        int seeScore = SEE::evaluate(board, move);
        if (seeScore >= 0) {
            int vic_val = (move.captured != NO_PIECE) ? getPieceValue(pieceType(move.captured)) : 100;
            int att_val = getPieceValue(pieceType(move.piece));
            return 20000 + (vic_val * 10 - att_val) + seeScore;
        } else {
            return 500 + (seeScore / 10);
        }
    }

    // 3. Queen promotions
    if (move.promotion != NO_PIECE) {
        if (pieceType(move.promotion) == QUEEN) return 18000;
        return 1000;
    }

    // 4. Killer moves
    if (ply < 64 && move == killerMoves[ply][0]) {
        return 16000;
    } else if (ply < 64 && move == killerMoves[ply][1]) {
        return 15000;
    }

    // 6. Countermove heuristic
    if (prevMove.from != SQ_NONE && move == counterMoveTable[prevMove.from][prevMove.to]) {
        return 14000;
    }

    // 5. History heuristic
    if (move.piece >= 0 && move.piece < 12 && move.to >= 0 && move.to < 64) {
        int hist = historyTable[move.piece][move.to];
        if (hist > 0) {
            return 1000 + std::min(hist, 12000);
        }
    }

    // 8. Remaining quiet moves
    return 0;
}

std::vector<Move> SearchWorker::returnSortedMoves(int ply, const Move& prevMove) {
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
    Move ttMove;
    TTEntry* entry = probeTT(board.zobristKey);
    if (entry) {
        ttMove = entry->bestMove;
    }

    std::sort(legalMoves.begin(), legalMoves.end(), [this, ply, ttMove, prevMove](const Move& a, const Move& b) {
        return scoreMove(a, ply, ttMove, prevMove) > scoreMove(b, ply, ttMove, prevMove);
    });
    return legalMoves;
}

class MovePicker {
public:
    enum Stage {
        STAGE_TT = 0,
        STAGE_GEN_CAPTURES,
        STAGE_CAPTURES,
        STAGE_GEN_QUIETS,
        STAGE_QUIETS,
        STAGE_GEN_EVASIONS,
        STAGE_EVASIONS,
        STAGE_DONE
    };

    SearchWorker& worker;
    Board& board;
    int ply;
    Move ttMove;
    Move prevMove;
    bool inCheck;
    Stage stage;
    std::vector<Move> moves;
    size_t moveIndex;

    MovePicker(SearchWorker& w, int p, const Move& tt, const Move& prev, bool check)
        : worker(w), board(w.board), ply(p), ttMove(tt), prevMove(prev), inCheck(check), stage(STAGE_TT), moveIndex(0) {
    }

    bool next(Move& move) {
        while (stage != STAGE_DONE) {
            switch (stage) {
                case STAGE_TT: {
                    stage = inCheck ? STAGE_GEN_EVASIONS : STAGE_GEN_CAPTURES;
                    if (ttMove.from != SQ_NONE && isLegalMove(board, ttMove)) {
                        move = ttMove;
                        return true;
                    }
                    break;
                }
                case STAGE_GEN_EVASIONS: {
                    moves = Movegen::generateLegalMoves(board, Movegen::GEN_ALL);
                    std::sort(moves.begin(), moves.end(), [this](const Move& m1, const Move& m2) {
                        return worker.scoreMove(m1, ply, ttMove, prevMove) > worker.scoreMove(m2, ply, ttMove, prevMove);
                    });
                    moveIndex = 0;
                    stage = STAGE_EVASIONS;
                    break;
                }
                case STAGE_EVASIONS: {
                    while (moveIndex < moves.size()) {
                        Move candidate = moves[moveIndex++];
                        if (candidate == ttMove) continue;
                        move = candidate;
                        return true;
                    }
                    stage = STAGE_DONE;
                    break;
                }
                case STAGE_GEN_CAPTURES: {
                    moves = Movegen::generateLegalMoves(board, Movegen::GEN_CAPTURES);
                    std::sort(moves.begin(), moves.end(), [this](const Move& m1, const Move& m2) {
                        return worker.scoreMove(m1, ply, ttMove, prevMove) > worker.scoreMove(m2, ply, ttMove, prevMove);
                    });
                    moveIndex = 0;
                    stage = STAGE_CAPTURES;
                    break;
                }
                case STAGE_CAPTURES: {
                    while (moveIndex < moves.size()) {
                        Move candidate = moves[moveIndex++];
                        if (candidate == ttMove) continue;
                        move = candidate;
                        return true;
                    }
                    stage = STAGE_GEN_QUIETS;
                    break;
                }
                case STAGE_GEN_QUIETS: {
                    moves = Movegen::generateLegalMoves(board, Movegen::GEN_QUIETS);
                    std::sort(moves.begin(), moves.end(), [this](const Move& m1, const Move& m2) {
                        return worker.scoreMove(m1, ply, ttMove, prevMove) > worker.scoreMove(m2, ply, ttMove, prevMove);
                    });
                    moveIndex = 0;
                    stage = STAGE_QUIETS;
                    break;
                }
                case STAGE_QUIETS: {
                    while (moveIndex < moves.size()) {
                        Move candidate = moves[moveIndex++];
                        if (candidate == ttMove) continue;
                        move = candidate;
                        return true;
                    }
                    stage = STAGE_DONE;
                    break;
                }
                case STAGE_DONE:
                    return false;
            }
        }
        return false;
    }
};

int SearchWorker::quiescence(int alpha, int beta, int ply) {
    searchedNodes++;
    if (ply > seldepth) seldepth = ply;
    checkTime(limits, searchedNodes);
    if (stopSearch.load(std::memory_order_relaxed)) {
        return 0;
    }

    if (ply > 0 && (board.isFiftyMoveDraw() || board.isRepetition(ply) || board.isInsufficientMaterial())) {
        return 0;
    }

    bool inCheck = Movegen::inCheck(board, board.getSideToMove());
    if (!inCheck) {
        int standPat = (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);
        if (standPat >= beta) {
            return beta;
        }
        if (standPat > alpha) {
            alpha = standPat;
        }
    }

    std::vector<Move> legalMoves;
    if (inCheck) {
        legalMoves = returnSortedMoves(ply, Move());
    } else {
        legalMoves = Movegen::generateLegalMoves(board, Movegen::GEN_CAPTURES);
        std::sort(legalMoves.begin(), legalMoves.end(), [this, ply](const Move& a, const Move& b) {
            return scoreMove(a, ply, Move(), Move()) > scoreMove(b, ply, Move(), Move());
        });
    }

    if (legalMoves.empty()) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        }
        return alpha;
    }

    for (const Move& move : legalMoves) {
        if (!inCheck && (move.captured != NO_PIECE || move.isEnPassant)) {
            if (!SEE::hasPositiveSEE(board, move, 0)) {
                continue;
            }
        }

        board.applyMove(move);
        int score = -quiescence(-beta, -alpha, ply + 1);
        board.undoMove(move);

        if (stopSearch.load(std::memory_order_relaxed)) {
            return 0;
        }

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return alpha;
}

int SearchWorker::negamax(int depth, int alpha, int beta, int ply, bool allowNull, Move excludedMove, Move prevMove) {
    searchedNodes++;
    if (ply > seldepth) seldepth = ply;
    checkTime(limits, searchedNodes);
    if (stopSearch.load(std::memory_order_relaxed)) {
        return 0;
    }

    if (ply >= 128) {
        return (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);
    }

    if (ply > 0 && (board.isFiftyMoveDraw() || board.isRepetition(ply) || board.isInsufficientMaterial())) {
        return 0;
    }

    if (ply > 0) {
        alpha = std::max(alpha, -MATE_SCORE + ply);
        beta = std::min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) {
            return alpha;
        }
    }

    bool inCheck = Movegen::inCheck(board, board.getSideToMove());
    if (inCheck && ply < 64) {
        depth++;
    }

    if (depth <= 0) {
        return quiescence(alpha, beta, ply);
    }

    TTEntry* entry = probeTT(board.zobristKey);
    Move ttMove;
    int ttScore = 0;
    bool ttHit = (entry != nullptr);
    if (ttHit) {
        ttMove = entry->bestMove;
        ttScore = scoreFromTT(entry->score, ply);
        if (ply > 0 && entry->depth >= depth && excludedMove.from == SQ_NONE) {
            if (entry->flag == TT_EXACT) {
                return ttScore;
            }
            if (entry->flag == TT_LOWERBOUND && ttScore >= beta) {
                return ttScore;
            }
            if (entry->flag == TT_UPPERBOUND && ttScore <= alpha) {
                return ttScore;
            }
        }
    }

    // Syzygy tablebase probing
    if (ply > 0 && Syzygy::isLoaded() && excludedMove.from == SQ_NONE) {
        Syzygy::WDLResult wdl = Syzygy::probeWDL(board);
        if (wdl != Syzygy::WDL_FAILED) {
            int tbScore = 0;
            if (wdl == Syzygy::WDL_WIN) {
                tbScore = MATE_SCORE - ply - 1;
            } else if (wdl == Syzygy::WDL_LOSS) {
                tbScore = -MATE_SCORE + ply + 1;
            } else if (wdl == Syzygy::WDL_CURSED_WIN) {
                tbScore = 200;
            } else if (wdl == Syzygy::WDL_BLESSED_LOSS) {
                tbScore = -200;
            } else {
                tbScore = 0;
            }

            if (tbScore >= beta) {
                storeTT(board.zobristKey, depth, tbScore, TT_LOWERBOUND, Move(), ply);
                return tbScore;
            }
            if (tbScore <= alpha) {
                storeTT(board.zobristKey, depth, tbScore, TT_UPPERBOUND, Move(), ply);
                return tbScore;
            }
            alpha = std::max(alpha, tbScore);
        }
    }

    int staticEval = (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);

    // 1.12 Razoring
    if (depth <= 3 && !inCheck && ply > 0 && excludedMove.from == SQ_NONE) {
        int razorMargin = 300 + 150 * depth;
        if (staticEval + razorMargin <= alpha) {
            int qScore = quiescence(alpha, beta, ply);
            if (qScore <= alpha) {
                return qScore;
            }
        }
    }

    // Null move pruning
    if (allowNull && !inCheck && depth >= 3 && ply > 0 && board.hasNonPawnMaterial(board.getSideToMove()) && excludedMove.from == SQ_NONE) {
        if (staticEval >= beta) {
            int R = 2 + (depth >= 6 ? 1 : 0);
            board.makeNullMove();
            int nullScore = -negamax(depth - 1 - R, -beta, -beta + 1, ply + 1, false, Move(), Move());
            board.undoNullMove();

            if (nullScore >= beta) {
                return beta;
            }
        }
    }

    // 1.13 Singular Extensions
    int extension = 0;
    if (ply > 0 && depth >= 6 && ttHit && ttMove.from != SQ_NONE &&
        (entry->flag == TT_LOWERBOUND || entry->flag == TT_EXACT) &&
        entry->depth >= depth - 3 && std::abs(ttScore) < MATE_THRESHOLD &&
        excludedMove.from == SQ_NONE) {

        int sBeta = ttScore - 2 * depth;
        int sScore = negamax(depth / 2, sBeta - 1, sBeta, ply, false, ttMove, prevMove);
        if (sScore < sBeta) {
            extension = 1;
        }
    }

    int originalAlpha = alpha;
    Move bestMoveFound;
    int movesSearched = 0;
    Move move;
    MovePicker picker(*this, ply, ttMove, prevMove, inCheck);

    while (picker.next(move)) {
        if (move == excludedMove) {
            continue;
        }

        bool isCapture = (move.captured != NO_PIECE || move.isEnPassant);
        bool isPromotion = (move.promotion != NO_PIECE);
        bool isQuiet = !isCapture && !isPromotion;

        // 1.11 Futility Pruning
        if (movesSearched > 0 && depth <= 2 && !inCheck && isQuiet && ply > 0 && excludedMove.from == SQ_NONE) {
            if (ply >= 64 || (move != killerMoves[ply][0] && move != killerMoves[ply][1])) {
                int margin = (depth == 1) ? 150 : 300;
                if (staticEval + margin <= alpha) {
                    continue;
                }
            }
        }

        board.applyMove(move);
        int score = 0;
        int moveExtension = (move == ttMove) ? extension : 0;
        int targetDepth = depth - 1 + moveExtension;

        if (movesSearched == 0) {
            score = -negamax(targetDepth, -beta, -alpha, ply + 1, true, Move(), move);
        } else {
            // 1.9 Late Move Reductions (LMR)
            bool canLMR = (depth >= 3 && movesSearched >= 3 && isQuiet && !inCheck &&
                           (ply >= 64 || (move != killerMoves[ply][0] && move != killerMoves[ply][1])));
            if (canLMR) {
                int R = 1 + (depth >= 6 ? 1 : 0) + (movesSearched >= 8 ? 1 : 0);
                int reducedDepth = std::max(1, targetDepth - R);
                score = -negamax(reducedDepth, -alpha - 1, -alpha, ply + 1, true, Move(), move);
                if (score > alpha) {
                    score = -negamax(targetDepth, -alpha - 1, -alpha, ply + 1, true, Move(), move);
                }
            } else {
                score = -negamax(targetDepth, -alpha - 1, -alpha, ply + 1, true, Move(), move);
            }

            if (score > alpha && score < beta) {
                score = -negamax(targetDepth, -beta, -alpha, ply + 1, true, Move(), move);
            }
        }

        board.undoMove(move);
        movesSearched++;

        if (stopSearch.load(std::memory_order_relaxed)) {
            return 0;
        }

        if (score >= beta) {
            if (isQuiet) {
                if (ply < 64) {
                    if (move != killerMoves[ply][0]) {
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = move;
                    }
                }
                if (prevMove.from != SQ_NONE) {
                    counterMoveTable[prevMove.from][prevMove.to] = move;
                }
                if (move.piece >= 0 && move.piece < 12 && move.to >= 0 && move.to < 64) {
                    historyTable[move.piece][move.to] += depth * depth;
                    if (historyTable[move.piece][move.to] > 100000) {
                        historyTable[move.piece][move.to] = 100000;
                    }
                }
            }
            if (excludedMove.from == SQ_NONE) {
                storeTT(board.zobristKey, depth, beta, TT_LOWERBOUND, move, ply);
            }
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            bestMoveFound = move;
        }
    }

    if (movesSearched == 0) {
        if (excludedMove.from != SQ_NONE) {
            return alpha;
        }
        if (inCheck) {
            return -MATE_SCORE + ply;
        } else {
            return 0;
        }
    }

    if (excludedMove.from == SQ_NONE) {
        TTFlag flag = (alpha > originalAlpha) ? TT_EXACT : TT_UPPERBOUND;
        storeTT(board.zobristKey, depth, alpha, flag, bestMoveFound, ply);
    }

    return alpha;
}

Move SearchWorker::searchRoot(bool isMainThread) {
    int maxDepth = (limits.depth > 0) ? limits.depth : 64;
    if (limits.depth <= 0 && limits.time <= 0) {
        maxDepth = 5;
    }

    std::vector<Move> legalMoves = returnSortedMoves(0, Move());
    if (legalMoves.empty()) {
        return Move();
    }

    // Syzygy tablebase DTZ probe at root (Main thread only)
    if (isMainThread && Syzygy::isLoaded()) {
        Move tbMove;
        int tbScore = 0;
        if (Syzygy::probeRoot(board, tbMove, tbScore)) {
            std::cout << "info depth 1";
            if (tbScore >= MATE_THRESHOLD) {
                int mateMoves = (MATE_SCORE - tbScore + 1) / 2;
                std::cout << " score mate " << mateMoves;
            } else if (tbScore <= -MATE_THRESHOLD) {
                int mateMoves = -(MATE_SCORE + tbScore + 1) / 2;
                std::cout << " score mate " << mateMoves;
            } else {
                std::cout << " score cp " << tbScore;
            }
            std::cout << " nodes 1 time 0 pv " << tbMove.toUCI() << "\n";
            std::cout.flush();
            bestMove = tbMove;
            return tbMove;
        }
    }

    bestMove = legalMoves[0];
    int prevScore = 0;
    int startDepth = isMainThread ? 1 : (1 + (threadId % 2));

    for (int currDepth = startDepth; currDepth <= maxDepth; ++currDepth) {
        int delta = 50;
        int alpha = (currDepth >= 4) ? std::max(-MATE_SCORE, prevScore - delta) : -MATE_SCORE;
        int beta  = (currDepth >= 4) ? std::min(MATE_SCORE, prevScore + delta) : MATE_SCORE;

        Move currentIterBestMove = legalMoves[0];
        bool completedMoveFound = false;
        int currentIterScore = 0;

        while (true) {
            int movesSearched = 0;
            int bestScore = -MATE_SCORE;
            int searchAlpha = alpha;

            for (Move move : legalMoves) {
                board.applyMove(move);
                int score = 0;
                int targetDepth = currDepth - 1;

                if (movesSearched == 0) {
                    score = -negamax(targetDepth, -beta, -searchAlpha, 1, true, Move(), move);
                } else {
                    score = -negamax(targetDepth, -searchAlpha - 1, -searchAlpha, 1, true, Move(), move);
                    if (score > searchAlpha && score < beta) {
                        score = -negamax(targetDepth, -beta, -searchAlpha, 1, true, Move(), move);
                    }
                }

                board.undoMove(move);
                movesSearched++;

                if (stopSearch.load(std::memory_order_relaxed)) {
                    break;
                }

                if (score > bestScore) {
                    bestScore = score;
                    currentIterBestMove = move;
                }

                if (score > searchAlpha) {
                    searchAlpha = score;
                }

                if (searchAlpha >= beta) {
                    break;
                }
            }

            if (stopSearch.load(std::memory_order_relaxed)) {
                break;
            }

            // Aspiration window bounds check
            if (currDepth >= 4) {
                if (bestScore <= alpha) {
                    alpha = std::max(-MATE_SCORE, alpha - delta);
                    delta *= 2;
                    if (delta > 800) {
                        alpha = -MATE_SCORE;
                        beta = MATE_SCORE;
                    }
                    continue;
                } else if (bestScore >= beta) {
                    beta = std::min(MATE_SCORE, beta + delta);
                    delta *= 2;
                    if (delta > 800) {
                        alpha = -MATE_SCORE;
                        beta = MATE_SCORE;
                    }
                    continue;
                }
            }

            currentIterScore = bestScore;
            completedMoveFound = true;
            break;
        }

        if (stopSearch.load(std::memory_order_relaxed)) {
            break;
        }

        if (completedMoveFound) {
            bestMove = currentIterBestMove;
            prevScore = currentIterScore;
            storeTT(board.zobristKey, currDepth, prevScore, TT_EXACT, bestMove, 0);

            if (isMainThread) {
                long long elapsed = limits.startTime > 0 ? (nowMs() - limits.startTime) : 0;
                long long totalNodes = ThreadPool::getTotalNodes();
                int maxSel = ThreadPool::getMaxSeldepth();
                long long nps = elapsed > 0 ? (totalNodes * 1000 / elapsed) : (totalNodes * 1000);
                int hashfull = getHashFull();
                std::vector<Move> pv = extractPV(board, currDepth);

                std::cout << "info depth " << currDepth
                          << " seldepth " << maxSel;
                if (prevScore >= MATE_THRESHOLD) {
                    int mateMoves = (MATE_SCORE - prevScore + 1) / 2;
                    std::cout << " score mate " << mateMoves;
                } else if (prevScore <= -MATE_THRESHOLD) {
                    int mateMoves = -(MATE_SCORE + prevScore + 1) / 2;
                    std::cout << " score mate " << mateMoves;
                } else {
                    std::cout << " score cp " << prevScore;
                }
                std::cout << " nodes " << totalNodes
                          << " nps " << nps
                          << " time " << elapsed
                          << " hashfull " << hashfull
                          << " pv";
                if (!pv.empty()) {
                    for (const Move& m : pv) {
                        std::cout << " " << m.toUCI();
                    }
                } else {
                    std::cout << " " << bestMove.toUCI();
                }
                std::cout << "\n";
                std::cout.flush();
            }
        }

        legalMoves = returnSortedMoves(0, Move());

        if (isMainThread && limits.time > 0) {
            long long elapsed = nowMs() - limits.startTime;
            if (elapsed >= limits.time * 0.5) {
                break;
            }
        }
    }

    return bestMove;
}

Move getBestMove(Board& board, const SearchLimits& limits) {
    return ThreadPool::startSearch(board, limits);
}

} // namespace Search
} // namespace Voraxis
