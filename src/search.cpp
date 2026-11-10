#include "search.h"
#include <iostream>
#include "board.h"
#include "evaluate.h"
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

    Move bestMove = legalMoves[0];

    for (int currDepth = 1; currDepth <= maxDepth; ++currDepth) {
        int alpha = -1000000;
        int beta = 1000000;
        Move currentIterBestMove = legalMoves[0];
        bool completedMoveFound = false;

        for (Move move : legalMoves) {
            board.applyMove(move);
            int score = -negamax(board, currDepth - 1, -beta, -alpha, limits, 1);
            board.undoMove(move);

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
        }

        // Move the best move to the front of legalMoves for the next iteration
        for (size_t i = 0; i < legalMoves.size(); ++i) {
            if (legalMoves[i] == bestMove) {
                std::swap(legalMoves[0], legalMoves[i]);
                break;
            }
        }

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
    std::sort(legalMoves.begin(), legalMoves.end(), [ply](const Move& a, const Move& b) {
        return scoreMove(a, ply) > scoreMove(b, ply);
    });
    return legalMoves;
}

int quiescence(Board& board, int alpha, int beta, const SearchLimits& limits) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
        return 0;
    }

    int standPat = (board.getSideToMove() == WHITE ? 1 : -1) * Evaluate::evaluate(board);
    if (standPat >= beta) {
        return beta;
    }
    if (standPat > alpha) {
        alpha = standPat;
    }

    std::vector<Move> legalMoves = returnSortedMoves(board, 0);
    for (const Move& move : legalMoves) {
        if (move.captured == NO_PIECE && move.promotion == NO_PIECE) {
            continue;
        }

        board.applyMove(move);
        int score = -quiescence(board, -beta, -alpha, limits);
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

int scoreMove(const Move move, int ply) {
    int score = 0;
    if (move.captured != NO_PIECE) {
        score = 10000;
        int vic_val = getPieceValue(pieceType(move.captured));
        int att_val = getPieceValue(pieceType(move.piece));
        score += vic_val * 10 - att_val;
    }
    if(move.captured == NO_PIECE){
        if(ply<64 && move == killerMoves[ply][0]){
            score = 9000;
        }else{

            if(ply<64 && move == killerMoves[ply][1]){
                score = 8000;
            }else{
                score = historyTable[move.piece][move.to];
            }
        }

    }
    if (move.promotion != NO_PIECE) {
        score += 9000;
    }
    return score;
}

int negamax(Board& board, int depth, int alpha, int beta, const SearchLimits& limits, int ply) {
    searchedNodes++;
    checkTime(limits);
    if (stopSearch) {
        return 0;
    }

    if (depth <= 0) {
        return quiescence(board, alpha, beta, limits);
    }

    std::vector<Move> legalMoves = returnSortedMoves(board, ply);

    if (legalMoves.empty()) {
        if (Movegen::inCheck(board, board.getSideToMove())) {
            return -1000000 - depth;
        } else {
            return 0;
        }
    }

    for (Move move : legalMoves) {
        board.applyMove(move);
        int score = -negamax(board, depth - 1, -beta, -alpha, limits, ply+1);
        board.undoMove(move);

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
                historyTable[move.piece][move.to] += depth * depth;
            }
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return alpha;
}

} // namespace Search
} // namespace Voraxis
