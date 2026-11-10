#include "search.h"
#include <iostream>
#include "board.h"
#include "evaluate.h"
#include "syzygy_bridge.h"
#include "see.h"
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
Move counterMoveTable[64][64];

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
    std::memset(counterMoveTable, 0, sizeof(counterMoveTable));
    std::vector<Move> legalMoves = returnSortedMoves(board, 0, Move());
    if (legalMoves.empty()) {
        return Move();
    }

    // Syzygy tablebase DTZ probe at root
    if (Syzygy::isLoaded()) {
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
            return tbMove;
        }
    }

    Move bestMove = legalMoves[0];
    int prevScore = 0;

    for (int currDepth = 1; currDepth <= maxDepth; ++currDepth) {
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

                if (movesSearched == 0) {
                    // First move searched with full window
                    score = -negamax(board, currDepth - 1, -beta, -searchAlpha, limits, 1, true, Move(), move);
                } else {
                    // Subsequent moves searched with null window
                    score = -negamax(board, currDepth - 1, -searchAlpha - 1, -searchAlpha, limits, 1, true, Move(), move);
                    // Re-search with full window if failed high
                    if (score > searchAlpha && score < beta) {
                        score = -negamax(board, currDepth - 1, -beta, -searchAlpha, limits, 1, true, Move(), move);
                    }
                }

                board.undoMove(move);
                movesSearched++;

                if (stopSearch) {
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

            if (stopSearch) {
                break;
            }

            // Aspiration window bounds check
            if (currDepth >= 4) {
                if (bestScore <= alpha) {
                    // Fail low: widen lower bound and re-search
                    alpha = std::max(-MATE_SCORE, alpha - delta);
                    delta *= 2;
                    if (delta > 800) {
                        alpha = -MATE_SCORE;
                        beta = MATE_SCORE;
                    }
                    continue;
                } else if (bestScore >= beta) {
                    // Fail high: widen upper bound and re-search
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

        if (stopSearch) {
            // Search was interrupted by timeout; keep best move from last completed depth
            break;
        }

        if (completedMoveFound) {
            bestMove = currentIterBestMove;
            prevScore = currentIterScore;
            storeTT(board.zobristKey, currDepth, prevScore, TT_EXACT, bestMove, 0);
            long long elapsed = limits.startTime > 0 ? (nowMs() - limits.startTime) : 0;
            std::cout << "info depth " << currDepth;
            if (prevScore >= MATE_THRESHOLD) {
                int mateMoves = (MATE_SCORE - prevScore + 1) / 2;
                std::cout << " score mate " << mateMoves;
            } else if (prevScore <= -MATE_THRESHOLD) {
                int mateMoves = -(MATE_SCORE + prevScore + 1) / 2;
                std::cout << " score mate " << mateMoves;
            } else {
                std::cout << " score cp " << prevScore;
            }
            std::cout << " nodes " << searchedNodes << " time " << elapsed << " pv " << bestMove.toUCI() << "\n";
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

std::vector<Move> returnSortedMoves(Board& board, int ply, const Move& prevMove) {
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
    Move ttMove;
    TTEntry* entry = probeTT(board.zobristKey);
    if (entry && entry->key == board.zobristKey) {
        ttMove = entry->bestMove;
    }

    std::sort(legalMoves.begin(), legalMoves.end(), [&board, ply, ttMove, prevMove](const Move& a, const Move& b) {
        return scoreMove(board, a, ply, ttMove, prevMove) > scoreMove(board, b, ply, ttMove, prevMove);
    });
    return legalMoves;
}

int quiescence(Board& board, int alpha, int beta, const SearchLimits& limits, int ply) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
        return 0;
    }

    // In-search draw detection (repetition, 50-move rule, insufficient material)
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

    std::vector<Move> legalMoves = returnSortedMoves(board, ply);
    if (legalMoves.empty()) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }

    for (const Move& move : legalMoves) {
        if (!inCheck && move.captured == NO_PIECE && move.promotion == NO_PIECE && !move.isEnPassant) {
            continue;
        }

        // SEE Pruning in Quiescence Search: prune losing captures (SEE < 0) when not in check
        if (!inCheck && (move.captured != NO_PIECE || move.isEnPassant)) {
            if (!SEE::hasPositiveSEE(board, move, 0)) {
                continue;
            }
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

int scoreMove(const Board& board, const Move move, int ply, const Move& ttMove, const Move& prevMove) {
    // 1. TT / Hash move
    if (ttMove.from != SQ_NONE && move == ttMove) {
        return 30000;
    }

    // Captures (Winning & equal captures vs Bad captures)
    if (move.captured != NO_PIECE || move.isEnPassant) {
        int seeScore = SEE::evaluate(board, move);
        if (seeScore >= 0) {
            // 2. Winning & equal captures (MVV-LVA: Most Valuable Victim - Least Valuable Attacker)
            int vic_val = (move.captured != NO_PIECE) ? getPieceValue(pieceType(move.captured)) : 100;
            int att_val = getPieceValue(pieceType(move.piece));
            return 20000 + (vic_val * 10 - att_val) + seeScore;
        } else {
            // 7. Bad captures (losing captures): ordered below countermoves and good quiets, but above remaining quiets
            // seeScore is negative (e.g. -100 to -900). Range: 410 to 490
            return 500 + (seeScore / 10);
        }
    }

    // 3. Queen promotions
    if (move.promotion != NO_PIECE) {
        if (move.promotion == W_QUEEN || move.promotion == B_QUEEN) {
            return 18000;
        }
        return 10000; // Underpromotions
    }

    // 4. Killer moves (quiet moves causing beta cutoffs at same ply)
    if (ply < 64 && move == killerMoves[ply][0]) {
        return 16000;
    } else if (ply < 64 && move == killerMoves[ply][1]) {
        return 15000;
    }

    // 6. Countermove heuristic (refutation move to opponent's previous move)
    if (prevMove.from != SQ_NONE && move == counterMoveTable[prevMove.from][prevMove.to]) {
        return 14000;
    }

    // 5. History heuristic (quiet moves indexed by [piece][to_square])
    if (move.piece >= 0 && move.piece < 12 && move.to >= 0 && move.to < 64) {
        int hist = historyTable[move.piece][move.to];
        if (hist > 0) {
            return 1000 + std::min(hist, 12000);
        }
    }

    // 8. Remaining quiet moves
    return 0;
}

int scoreMove(const Board& board, const Move move, int ply) {
    return scoreMove(board, move, ply, Move(), Move());
}

int negamax(Board& board, int depth, int alpha, int beta, const SearchLimits& limits, int ply, bool allowNull, Move excludedMove, Move prevMove) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
        return 0;
    }

    // 1.14 Maximum ply limit
    if (ply >= 128) {
        return (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);
    }

    // In-search draw detection (repetition, 50-move rule, insufficient material)
    if (ply > 0 && (board.isFiftyMoveDraw() || board.isRepetition(ply) || board.isInsufficientMaterial())) {
        return 0;
    }

    // 1.14 Mate Distance Pruning
    if (ply > 0) {
        alpha = std::max(alpha, -MATE_SCORE + ply);
        beta = std::min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) {
            return alpha;
        }
    }

    // 1.8 Check Extensions: extend search depth by 1 ply when in check
    bool inCheck = Movegen::inCheck(board, board.getSideToMove());
    if (inCheck && ply < 64) {
        depth++;
    }

    if (depth <= 0) {
        return quiescence(board, alpha, beta, limits, ply);
    }

    // TT Probe & Cutoff (skip cutoff at root ply == 0 or when a move is excluded)
    TTEntry* entry = probeTT(board.zobristKey);
    Move ttMove;
    int ttScore = 0;
    bool ttHit = (entry && entry->key == board.zobristKey);
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

    // Syzygy WDL Probe during search (instant game-theoretic cutoff)
    if (ply > 0 && Syzygy::isLoaded() && excludedMove.from == SQ_NONE) {
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

    int staticEval = inCheck ? (-MATE_SCORE + ply) : ((board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board));

    // 1.12 Razoring: Early quiescence drop at shallow pre-frontier nodes if eval is far below alpha
    if (!inCheck && depth <= 3 && ply > 0 && excludedMove.from == SQ_NONE) {
        const int razorMargin[4] = {0, 200, 350, 500};
        if (staticEval + razorMargin[depth] <= alpha) {
            int rScore = quiescence(board, alpha, beta, limits, ply);
            if (rScore <= alpha) {
                return rScore;
            }
        }
    }

    // 1.10 Null Move Pruning (NMP)
    if (allowNull && depth >= 3 && ply > 0 && !inCheck && excludedMove.from == SQ_NONE) {
        if (board.hasNonPawnMaterial(board.getSideToMove())) {
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

    // 1.13 Singular Extensions: extend search if TT move is significantly better than all alternatives
    int extension = 0;
    if (ply > 0 && depth >= 6 && ttHit && ttMove.from != SQ_NONE &&
        (entry->flag == TT_LOWERBOUND || entry->flag == TT_EXACT) &&
        entry->depth >= depth - 3 && std::abs(ttScore) < MATE_THRESHOLD &&
        excludedMove.from == SQ_NONE) {

        int sBeta = ttScore - 2 * depth;
        int sScore = negamax(board, depth / 2, sBeta - 1, sBeta, limits, ply, false, ttMove, prevMove);
        if (sScore < sBeta) {
            extension = 1;
        }
    }

    std::vector<Move> legalMoves = returnSortedMoves(board, ply, prevMove);

    if (legalMoves.empty()) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        } else {
            return 0;
        }
    }

    int originalAlpha = alpha;
    Move bestMove;
    int movesSearched = 0;

    for (Move move : legalMoves) {
        if (move == excludedMove) {
            continue;
        }

        bool isCapture = (move.captured != NO_PIECE || move.isEnPassant);
        bool isPromotion = (move.promotion != NO_PIECE);
        bool isQuiet = !isCapture && !isPromotion;

        // 1.11 Futility Pruning: prune hopeless quiet moves near leaf nodes if eval + margin < alpha
        if (movesSearched > 0 && depth <= 2 && !inCheck && isQuiet && ply > 0 && excludedMove.from == SQ_NONE) {
            if (ply >= 64 || (move != killerMoves[ply][0] && move != killerMoves[ply][1])) {
                int margin = (depth == 1) ? 150 : 300;
                if (staticEval + margin <= alpha) {
                    continue; // Prune quiet move
                }
            }
        }

        board.applyMove(move);
        int score = 0;
        int moveExtension = (move == ttMove) ? extension : 0;
        int targetDepth = depth - 1 + moveExtension;

        if (movesSearched == 0) {
            // First move searched with full window
            score = -negamax(board, targetDepth, -beta, -alpha, limits, ply + 1, true, Move(), move);
        } else {
            // 1.9 Late Move Reductions (LMR)
            bool canLMR = (depth >= 3 && movesSearched >= 3 && isQuiet && !inCheck &&
                           (ply >= 64 || (move != killerMoves[ply][0] && move != killerMoves[ply][1])));
            if (canLMR) {
                int R = 1 + (depth >= 6 ? 1 : 0) + (movesSearched >= 8 ? 1 : 0);
                int reducedDepth = std::max(1, targetDepth - R);
                score = -negamax(board, reducedDepth, -alpha - 1, -alpha, limits, ply + 1, true, Move(), move);
                if (score > alpha) {
                    // Re-search at full depth with null window
                    score = -negamax(board, targetDepth, -alpha - 1, -alpha, limits, ply + 1, true, Move(), move);
                }
            } else {
                score = -negamax(board, targetDepth, -alpha - 1, -alpha, limits, ply + 1, true, Move(), move);
            }

            // Re-search with full window if failed high
            if (score > alpha && score < beta) {
                score = -negamax(board, targetDepth, -beta, -alpha, limits, ply + 1, true, Move(), move);
            }
        }

        board.undoMove(move);
        movesSearched++;

        if (stopSearch) {
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
            bestMove = move;
        }
    }

    if (excludedMove.from != SQ_NONE && movesSearched == 0) {
        return alpha;
    }

    if (excludedMove.from == SQ_NONE) {
        TTFlag flag = (alpha > originalAlpha) ? TT_EXACT : TT_UPPERBOUND;
        storeTT(board.zobristKey, depth, alpha, flag, bestMove, ply);
    }

    return alpha;
}

} // namespace Search
} // namespace Voraxis
