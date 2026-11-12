#include "uci.h"
#include "movegen.h"
#include "search.h"
#include "threadpool.h"
#include "book.h"
#include "syzygy_bridge.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace Voraxis {
namespace UCI {

UCIOptions options;

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
        std::cout << "option name Hash type spin default 64 min 1 max 2048\n";
        std::cout << "option name Clear Hash type button\n";
        std::cout << "option name Threads type spin default 1 min 1 max 128\n";
        std::cout << "option name Move Overhead type spin default 10 min 0 max 5000\n";
        std::cout << "option name OwnBook type check default true\n";
        std::cout << "option name BookPath type string default books/komodo.bin\n";
        std::cout << "option name SyzygyPath type string default books/syzygy\n";
        std::cout << "uciok\n";
        std::cout.flush();
    } else if (command == "setoption") {
        std::string token, optName, optValue;
        while (iss >> token) {
            if (token == "name") {
                optName = "";
                while (iss >> token && token != "value") {
                    if (!optName.empty()) optName += " ";
                    optName += token;
                }
            }
            if (token == "value") {
                std::getline(iss, optValue);
                size_t first = optValue.find_first_not_of(" \t\r\n");
                if (first != std::string::npos) {
                    optValue = optValue.substr(first);
                    size_t last = optValue.find_last_not_of(" \t\r\n");
                    optValue = optValue.substr(0, last + 1);
                } else {
                    optValue = "";
                }
                break;
            }
        }

        std::string lowerName = optName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName == "hash") {
            try {
                int mb = std::stoi(optValue);
                options.hashMB = mb;
                Search::setTTSize(mb);
            } catch (...) {}
        } else if (lowerName == "clear hash") {
            Search::clearTT();
        } else if (lowerName == "threads") {
            try {
                options.threads = std::max(1, std::min(128, std::stoi(optValue)));
                ThreadPool::setThreadCount(options.threads);
            } catch (...) {}
        } else if (lowerName == "move overhead") {
            try {
                options.moveOverhead = std::max(0, std::min(5000, std::stoi(optValue)));
            } catch (...) {}
        } else if (lowerName == "ownbook") {
            std::string lowerVal = optValue;
            std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), ::tolower);
            options.ownBook = (lowerVal == "true" || lowerVal == "1");
        } else if (lowerName == "bookpath") {
            options.bookPath = optValue;
            Book::init(optValue);
        } else if (lowerName == "syzygypath") {
            options.syzygyPath = optValue;
            Syzygy::init(optValue);
        }
    } else if (command == "isready") {
        std::cout << "readyok\n";
        std::cout.flush();
    } else if (command == "ucinewgame") {
        board.reset();
        Search::clearTT();
    } else if (command == "position") {
        parsePosition(iss, board);
    } else if (command == "stop") {
        ThreadPool::stop();
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
            limits.time = std::max(10, movetime - options.moveOverhead);
            limits.optimumTime = limits.time;
        } else if (wtime >= 0 || btime >= 0) {
            int myTime = (board.getSideToMove() == WHITE) ? wtime : btime;
            int myInc = (board.getSideToMove() == WHITE) ? winc : binc;
            myTime = std::max(10, myTime - options.moveOverhead);
            if (myTime > 0) {
                if (movestogo > 0) {
                    limits.optimumTime = (myTime / (movestogo + 1)) + (myInc * 3 / 4);
                } else {
                    limits.optimumTime = (myTime / 25) + (myInc / 2);
                }
                limits.optimumTime = std::min(limits.optimumTime, myTime / 2);
                limits.optimumTime = std::max(10, limits.optimumTime);
                limits.time = std::min(myTime / 2, limits.optimumTime * 5 / 2);
                limits.time = std::max(limits.time, limits.optimumTime);
            }
        }

        if (depth > 0) {
            limits.depth = depth;
        }

        // 1. Try opening book move first (if OwnBook is enabled, up to move 25)
        if (options.ownBook) {
            Move bookMove = Book::getBookMove(board, 25);
            if (bookMove.from != SQ_NONE) {
                std::cout << "bestmove " << bookMove.toUCI() << "\n";
                std::cout.flush();
                return;
            }
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
