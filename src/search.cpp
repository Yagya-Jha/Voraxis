#include "search.h"
#include <iostream>
#include "board.h"
//#include <climits>
#include "evaluate.h"
#include <algorithm>

#define MIN -1000000
#define MAX 1000000

namespace Voraxis {
namespace Search {

Move getBestMove(Board& board) {
    const int maxDepth = 4;
    std::vector<Move> legalMoves = returnSortedMoves(board);
    if (legalMoves.empty()) {
        return Move();
    }
/*MINIMAX APPROACH <- NO LONGER BEING USED ->

Move m = legalMoves[0];
int bestScore1 = MIN;
int bestScore2 = MAX;
if(board.getSideToMove()==WHITE){
        for(Move move:legalMoves){
            board.applyMove(move);
            int score = minimax(board,depth-1,false);
            if(score > bestScore1){
                m = move;
                bestScore1 = score;
            }
            board.undoMove(move);

        }
    }else{
        for(Move move: legalMoves){
            board.applyMove(move);
            int score = minimax(board, depth-1, true);
            if(score < bestScore2){
                m = move;
                bestScore2 = score;
            }
            board.undoMove(move);
        }
    }

    return m;
    
    */

int bestScore = MIN;
Move m = legalMoves[0];
for(int currDepth=1; currDepth <= maxDepth; ++currDepth){
    int alpha = -1000000; int beta = 1000000;
    for(Move move: legalMoves){
        board.applyMove(move);
        int score = -negamax(board, currDepth-1, -beta, -alpha);
        board.undoMove(move);
        if(score > alpha){
            alpha = score;
            m=move;
        }
    }
}
return m;
}

std::vector<Move> returnSortedMoves(Board& board){
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
    // Sort moves in descending order (highest score first)
    std::sort(legalMoves.begin(), legalMoves.end(), [](const Move& a, const Move& b) {
        return scoreMove(a) > scoreMove(b);
    });
    return legalMoves;
}

int quiescence(Board& board, int alpha, int beta){
    int standPat = (board.getSideToMove()==WHITE?1:-1) * Evaluate::evaluate(board);
    if(standPat >= beta){ 
        return beta;
    }
    if(standPat > alpha){
        alpha = standPat;
    }
    std::vector<Move> legalMoves = returnSortedMoves(board);
    for(const Move& move: legalMoves){
        if(move.captured==NO_PIECE && move.promotion == NO_PIECE){
            continue;
        }
        board.applyMove(move);
        int score = -quiescence(board, -beta, -alpha);
        board.undoMove(move);
        if(score >= beta){
            return beta;
        }
        if(score > alpha){
            alpha = score;
        }
    }
    return alpha;

}

int getPieceValue(int pType){
    constexpr int arr[] = {100, 300, 350, 500, 900, 10000};
    return arr[pType];
}

//score for MVVLVA
int scoreMove(const Move move){
    int score = 0;
    if (move.captured != NO_PIECE){
        score = 10000;
        int vic_val = getPieceValue(pieceType(move.captured));
        int att_val = getPieceValue(pieceType(move.piece));
        score +=vic_val*10 - att_val;
    }
    if(move.promotion != NO_PIECE){
        score +=9000;
    }
    return score;
}

int negamax(Board& board, int depth, int alpha, int beta){
    if(depth==0){
        return quiescence(board, alpha, beta);
    }

    std::vector<Move> legalMoves = returnSortedMoves(board);

    if (legalMoves.empty()) {
        if (Movegen::inCheck(board, board.getSideToMove())) {
            // Checkmate detected
            return -1000000-depth;
        } else {
            // Stalemate detected
            return 0;
        }
    }

    int score = -1000000;
    for(Move move: legalMoves){
        board.applyMove(move);
        score = -negamax(board, depth-1, -beta, -alpha);
        board.undoMove(move);
        if(score >= beta){
            return beta;
        }
        if(score > alpha){
            alpha = score;
        }
    }
    return alpha;
}
/*
int minimax(Board& board, int depth, bool isMaximizing) {
    if(depth==0){
        return Evaluate::evaluate(board);
    }
    std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);

    if (legalMoves.empty()) {
        if (Movegen::inCheck(board, board.getSideToMove())) {
            // Checkmate detected
            return (board.getSideToMove()==WHITE?-1000000-depth:1000000+depth);
        } else {
            // Stalemate detected
            return 0;
        }
    }


    
    if(isMaximizing){
    int bestScore = MIN;
        for(Move move: legalMoves){
            board.applyMove(move);
            bestScore = std::max(bestScore, minimax(board, depth-1, false));
            board.undoMove(move);
        }
        return bestScore;
    }else{
        int bestScore = MAX;
        for(Move move: legalMoves){
            board.applyMove(move);
            bestScore = std::min(bestScore, minimax(board, depth-1, true));
            board.undoMove(move);
        }
        return bestScore;
    }
}
*/
} // namespace Search
} // namespace Voraxis
