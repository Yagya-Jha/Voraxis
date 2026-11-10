#include "uci.h"
#include "movegen.h"
#include "search.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace Voraxis {
namespace UCI {

static void parsePosition(std::istringstream& iss, Board& board) {
    std::string token;
    if (!(iss >> token)) return;

    if (token == "startpos") {
        board.reset();
        iss >> token; // Might be "moves" or nothing
    } else if (token == "fen") {
        std::string fen;
        while (iss >> token && token != "moves") {
            if (!fen.empty()) fen += " ";
            fen += token;
        }
        board.loadFEN(fen);
    }

    if (token == "moves") {
        std::string moveStr;
        while (iss >> moveStr) {
            std::vector<Move> legalMoves = Movegen::generateLegalMoves(board);
            bool found = false;
            for (const auto& m : legalMoves) {
                if (m.toUCI() == moveStr) {
                    board.applyMove(m);
                    found = true;
                    break;
                }
            }
            if (!found) {
                Move m = Move::fromUCI(moveStr, board);
                if (m.from != SQ_NONE) {
                    board.applyMove(m);
                }
            }
        }
    }
}

void handleCommand(const std::string& line, Board& board, bool& running) {
    std::istringstream iss(line);
    std::string command;
    if (!(iss >> command)) return;

    if (command == "uci") {
        std::cout << "id name Voraxis 1.0\n";
        std::cout << "id author Voraxis Team\n";
        std::cout << "uciok\n";
        std::cout.flush();
    } else if (command == "isready") {
        std::cout << "readyok\n";
        std::cout.flush();
    } else if (command == "ucinewgame") {
        board.reset();
        Search::clearTT();
    } else if (command == "position") {
        parsePosition(iss, board);
    } else if (command == "stop") {
        Search::stopSearch = true;
    } else if (command == "go") {
        Search::SearchLimits limits;
        int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = -1, movetime = -1, depth = -1;
        std::string token;
        while (iss >> token) {
            if (token == "wtime") iss >> wtime;
            else if (token == "btime") iss >> btime;
            else if (token == "winc") iss >> winc;
            else if (token == "binc") iss >> binc;
            else if (token == "movestogo") iss >> movestogo;
            else if (token == "movetime") iss >> movetime;
            else if (token == "depth") iss >> depth;
        }

        if (movetime > 0) {
            limits.time = std::max(10, movetime - 15);
        } else if (wtime >= 0 || btime >= 0) {
            int myTime = (board.getSideToMove() == WHITE) ? wtime : btime;
            int myInc = (board.getSideToMove() == WHITE) ? winc : binc;
            if (myTime > 0) {
                if (movestogo > 0) {
                    limits.time = (myTime / (movestogo + 1)) + (myInc * 3 / 4);
                } else {
                    limits.time = (myTime / 25) + (myInc / 2);
                }
                limits.time = std::min(limits.time, myTime / 2);
                limits.time = std::max(10, limits.time);
            }
        }

        if (depth > 0) {
            limits.depth = depth;
        }

        limits.startTime = Search::nowMs();

        Move bestMove = Search::getBestMove(board, limits);
        if (bestMove.from != SQ_NONE) {
            std::cout << "bestmove " << bestMove.toUCI() << "\n";
        } else {
            std::cout << "bestmove 0000\n";
        }
        std::cout.flush();
    } else if (command == "perft") {
        int depth = 1;
        iss >> depth;
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t nodes = Movegen::perft(board, depth);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "Depth " << depth << ": " << nodes << " nodes in " << ms << " ms\n";
        std::cout.flush();
    } else if (command == "test") {
        Movegen::runPerftSuite();
    } else if (command == "d" || command == "print") {
        board.print();
    } else if (command == "quit") {
        running = false;
    }
}

void loop() {
    Board board;
    bool running = true;
    std::string line;

    while (running && std::getline(std::cin, line)) {
        handleCommand(line, board, running);
    }
}

} // namespace UCI
} // namespace Voraxis
