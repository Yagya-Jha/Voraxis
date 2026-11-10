#include <iostream>
#include <string>
#include "board.h"
#include "movegen.h"
#include "evaluate.h"
#include "book.h"
#include "syzygy_bridge.h"
#include "uci.h"

int main(int argc, char* argv[]) {
    // Initialize lookup tables and zobrist keys
    Voraxis::Movegen::initAttackTables();
    Voraxis::Evaluate::initEvaluation();
    Voraxis::Board::initZobrist();
    Voraxis::Book::init("books/komodo.bin");

    // Initialize Syzygy tablebases if available
    if (!Voraxis::Syzygy::init("books/syzygy")) {
        Voraxis::Syzygy::init("../books/syzygy");
    }

    // Check command line arguments for direct testing
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "test" || arg == "--test") {
            bool passed = Voraxis::Movegen::runPerftSuite();
            return passed ? 0 : 1;
        } else if (arg == "perft" && argc > 2) {
            int depth = std::stoi(argv[2]);
            Voraxis::Board board;
            uint64_t nodes = Voraxis::Movegen::perft(board, depth);
            std::cout << "Perft(" << depth << ") = " << nodes << "\n";
            return 0;
        }
    }

    // Start UCI command loop
    Voraxis::UCI::loop();

    return 0;
}

//g++ -O3 -std=c++17 src/main.cpp src/board.cpp src/movegen.cpp src/evaluate.cpp src/search.cpp src/uci.cpp -o build/voraxis.exeg++ -O3 -std=c++17 src/main.cpp src/board.cpp src/movegen.cpp src/evaluate.cpp src/search.cpp src/uci.cpp -o build/voraxis.exe