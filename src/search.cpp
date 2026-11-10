#include "search.h"
#include <iostream>
#include "board.h"
#include "evaluate.h"
#include "syzygy_bridge.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#define MIN -1000000
#define MAX 1000000

namespace Voraxis {
namespace Search {

bool stopSearch = false;
long long searchedNodes = 0;
int historyTable[12][64];
Move killerMoves[64][2];

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
    size_t index = key & (TT_ENTRIES - 1);
    if (ttTable[index].key == key) {
        return &ttTable[index];
    }
    return nullptr;
}

void storeTT(uint64_t key, int depth, int score, TTFlag flag, Move bestMove, int ply) {
    size_t index = key & (TT_ENTRIES - 1);
    TTEntry& entry = ttTable[index];

    if (entry.key == 0 || entry.key == key || depth >= entry.depth) {
        entry.key = key;
        entry.depth = depth;
        entry.score = scoreToTT(score, ply);
        entry.flag = flag;
        if (bestMove.from != SQ_NONE) {
            entry.bestMove = bestMove;
        }
    }
}

void clearTT() {
    std::fill(ttTable.begin(), ttTable.end(), TTEntry{});
}

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void checkTime(const SearchLimits& limits) {
    if (limits.time > 0 && (searchedNodes & 2047) == 0) {
        if (nowMs() - limits.startTime >= limits.time) {
            stopSearch = true;
        }
    }
}

Move getBestMove(Board& board, const SearchLimits& limits) {
    stopSearch = false;
    searchedNodes = 0;

    int maxDepth = 64;
    if (limits.depth > 0) {
        maxDepth = limits.depth;
    } else if (limits.time <= 0) {
        maxDepth = 5; // Default depth if no time or depth limit specified
    }
    std::memset(killerMoves, 0, sizeof(killerMoves));
    std::memset(historyTable, 0, sizeof(historyTable));
    std::vector<Move> legalMoves = returnSortedMoves(board, 1);
    if (legalMoves.empty()) {
        return Move();
    }

    // Syzygy tablebase DTZ probe at root
    if (Syzygy::isLoaded()) {
        Move tbMove;
        int tbScore = 0;
        if (Syzygy::probeRoot(board, tbMove, tbScore)) {
            std::cout << "info depth 1 score cp " << tbScore << " nodes 1 time 0 pv " << tbMove.toUCI() << "\n";
            std::cout.flush();
            return tbMove;
        }
    }

    Move bestMove = legalMoves[0];

    for (int currDepth = 1; currDepth <= maxDepth; ++currDepth) {
        int alpha = -1000000;
        int beta = 1000000;
        Move currentIterBestMove = legalMoves[0];
        bool completedMoveFound = false;

        int movesSearched = 0;
        for (Move move : legalMoves) {
            board.applyMove(move);
            int score = 0;

            if (movesSearched == 0) {
                // First move searched with full window
                score = -negamax(board, currDepth - 1, -beta, -alpha, limits, 1);
            } else {
                // Subsequent moves searched with null window
                score = -negamax(board, currDepth - 1, -alpha - 1, -alpha, limits, 1);
                // Re-search with full window if failed high
                if (score > alpha && score < beta) {
                    score = -negamax(board, currDepth - 1, -beta, -alpha, limits, 1);
                }
            }

            board.undoMove(move);
            movesSearched++;

            if (stopSearch) {
                break;
            }

            if (score > alpha) {
                alpha = score;
                currentIterBestMove = move;
                completedMoveFound = true;
            }
        }

        if (stopSearch) {
            // Search was interrupted by timeout; keep best move from last completed depth
            break;
        }

        if (completedMoveFound) {
            bestMove = currentIterBestMove;
            storeTT(board.zobristKey, currDepth, alpha, TT_EXACT, bestMove, 0);
            long long elapsed = limits.startTime > 0 ? (nowMs() - limits.startTime) : 0;
            std::cout << "info depth " << currDepth << " score cp " << alpha << " nodes " << searchedNodes << " time " << elapsed << " pv " << bestMove.toUCI() << "\n";
            std::cout.flush();
        }

        // Re-sort root moves using updated TT move and history heuristics
        legalMoves = returnSortedMoves(board, 0);

        // If we have already spent more than 50% of the allocated time budget,
        // do not start a new iteration since it will almost certainly exceed time limit
        if (limits.time > 0) {
            long long elapsed = nowMs() - limits.startTime;
            if (elapsed >= limits.time * 0.5) {
                break;
            }
        }
    }

    return bestMove;
}

std::vector<Move> returnSortedMoves(Board& board, int ply) {
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
    Move ttMove;
    TTEntry* entry = probeTT(board.zobristKey);
    if (entry && entry->key == board.zobristKey) {
        ttMove = entry->bestMove;
    }

    std::sort(legalMoves.begin(), legalMoves.end(), [ply, ttMove](const Move& a, const Move& b) {
        return scoreMove(a, ply, ttMove) > scoreMove(b, ply, ttMove);
    });
    return legalMoves;
}

int quiescence(Board& board, int alpha, int beta, const SearchLimits& limits, int ply) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
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

    std::vector<Move> legalMoves = returnSortedMoves(board, ply);
    if (legalMoves.empty()) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }

    for (const Move& move : legalMoves) {
        if (!inCheck && move.captured == NO_PIECE && move.promotion == NO_PIECE) {
            continue;
        }

        board.applyMove(move);
        int score = -quiescence(board, -beta, -alpha, limits, ply + 1);
        board.undoMove(move);

        if (stopSearch) {
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

int getPieceValue(int pType) {
    constexpr int arr[] = {100, 300, 350, 500, 900, 10000};
    return arr[pType];
}

int scoreMove(const Move move, int ply, const Move& ttMove) {
    if (ttMove.from != SQ_NONE && move == ttMove) {
        return 30000;
    }
    int score = 0;
    if (move.captured != NO_PIECE) {
        score = 10000;
        int vic_val = getPieceValue(pieceType(move.captured));
        int att_val = getPieceValue(pieceType(move.piece));
        score += vic_val * 10 - att_val;
    }
    if (move.captured == NO_PIECE) {
        if (ply < 64 && move == killerMoves[ply][0]) {
            score = 9000;
        } else {
            if (ply < 64 && move == killerMoves[ply][1]) {
                score = 8000;
            } else {
                if (move.piece >= 0 && move.piece < 12 && move.to >= 0 && move.to < 64) {
                    score = historyTable[move.piece][move.to];
                }
            }
        }
    }
    if (move.promotion != NO_PIECE) {
        score += 9000;
    }
    return score;
}

int scoreMove(const Move move, int ply) {
    return scoreMove(move, ply, Move());
}

int negamax(Board& board, int depth, int alpha, int beta, const SearchLimits& limits, int ply, bool allowNull) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
        return 0;
    }

    // In-search draw detection (repetition, 50-move rule, insufficient material)
    if (ply > 0 && (board.isFiftyMoveDraw() || board.isRepetition(ply) || board.isInsufficientMaterial())) {
        return 0;
    }

    // TT Probe & Cutoff (skip cutoff at root ply == 0)
    TTEntry* entry = probeTT(board.zobristKey);
    if (entry && entry->key == board.zobristKey && ply > 0) {
        if (entry->depth >= depth) {
            int ttScore = scoreFromTT(entry->score, ply);
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

    // Syzygy WDL Probe during search (instant game-theoretic cutoff)
    if (ply > 0 && Syzygy::isLoaded()) {
        uint64_t occAll = board.occBB[WHITE] | board.occBB[BLACK];
        int pieceCount = __builtin_popcountll(occAll);
        if (pieceCount <= Syzygy::getMaxPieces() && board.castlingRights == NO_CASTLING) {
            Syzygy::WDLResult wdl = Syzygy::probeWDL(board);
            if (wdl != Syzygy::WDL_FAILED) {
                int tbScore = 0;
                if (wdl == Syzygy::WDL_WIN) {
                    tbScore = MATE_THRESHOLD - 1000 - ply;
                } else if (wdl == Syzygy::WDL_LOSS) {
                    tbScore = -(MATE_THRESHOLD - 1000) + ply;
                } else if (wdl == Syzygy::WDL_CURSED_WIN) {
                    tbScore = 1;
                } else if (wdl == Syzygy::WDL_BLESSED_LOSS) {
                    tbScore = -1;
                } else {
                    tbScore = 0;
                }

                if (tbScore >= beta) {
                    storeTT(board.zobristKey, depth, beta, TT_LOWERBOUND, Move(), ply);
                    return beta;
                }
                if (tbScore <= alpha) {
                    storeTT(board.zobristKey, depth, alpha, TT_UPPERBOUND, Move(), ply);
                    return alpha;
                }
                storeTT(board.zobristKey, depth, tbScore, TT_EXACT, Move(), ply);
                return tbScore;
            }
        }
    }

    // Null Move Pruning (NMP)
    if (allowNull && depth >= 3 && ply > 0 && !Movegen::inCheck(board, board.getSideToMove())) {
        if (board.hasNonPawnMaterial(board.getSideToMove())) {
            int staticEval = (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);
            if (staticEval >= beta) {
                int R = (depth >= 6) ? 3 : 2;

                board.makeNullMove();
                int score = -negamax(board, depth - 1 - R, -beta, -beta + 1, limits, ply + 1, false);
                board.undoNullMove();

                if (stopSearch) {
                    return 0;
                }

                if (score >= beta) {
                    return beta;
                }
            }
        }
    }

    if (depth <= 0) {
        return quiescence(board, alpha, beta, limits, ply);
    }

    std::vector<Move> legalMoves = returnSortedMoves(board, ply);

    if (legalMoves.empty()) {
        if (Movegen::inCheck(board, board.getSideToMove())) {
            return -MATE_SCORE + ply;
        } else {
            return 0;
        }
    }

    int originalAlpha = alpha;
    Move bestMove;
    int movesSearched = 0;

    for (Move move : legalMoves) {
        board.applyMove(move);
        int score = 0;

        if (movesSearched == 0) {
            // First move searched with full window
            score = -negamax(board, depth - 1, -beta, -alpha, limits, ply + 1);
        } else {
            // Subsequent moves searched with zero / null window
            score = -negamax(board, depth - 1, -alpha - 1, -alpha, limits, ply + 1);
            // Re-search with full window if failed high
            if (score > alpha && score < beta) {
                score = -negamax(board, depth - 1, -beta, -alpha, limits, ply + 1);
            }
        }

        board.undoMove(move);
        movesSearched++;

        if (stopSearch) {
            return 0;
        }

        if (score >= beta) {
            if (move.captured == NO_PIECE) {
                if (ply < 64) {
                    if (move != killerMoves[ply][0]) {
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = move;
                    }
                }
                if (move.piece >= 0 && move.piece < 12 && move.to >= 0 && move.to < 64) {
                    historyTable[move.piece][move.to] += depth * depth;
                }
            }
            storeTT(board.zobristKey, depth, beta, TT_LOWERBOUND, move, ply);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }

    TTFlag flag = (alpha > originalAlpha) ? TT_EXACT : TT_UPPERBOUND;
    storeTT(board.zobristKey, depth, alpha, flag, bestMove, ply);

    return alpha;
}

} // namespace Search
} // namespace Voraxis
