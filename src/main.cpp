#include <iostream>
#include <string>
#include "board.h"
#include "movegen.h"
#include "book.h"
#include "syzygy_bridge.h"
#include "threadpool.h"
#include "evaluate.h"
#include "uci.h"

int main(int argc, char* argv[]) {

    Voraxis::Movegen::initAttackTables();
    Voraxis::Board::initZobrist();
    Voraxis::Evaluate::init();
    Voraxis::Book::init("books/komodo.bin");
    Voraxis::ThreadPool::init(1);

    if (!Voraxis::Syzygy::init("books/syzygy")) {
        Voraxis::Syzygy::init("../books/syzygy");
    }

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "test" || arg == "--test") {
            bool passed = Voraxis::Movegen::runPerftSuite();
            Voraxis::ThreadPool::cleanup();
            return passed ? 0 : 1;
        } else if (arg == "perft" && argc > 2) {
            int depth = std::stoi(argv[2]);
            Voraxis::Board board;
            uint64_t nodes = Voraxis::Movegen::perft(board, depth);
            std::cout << "Perft(" << depth << ") = " << nodes << "\n";
            Voraxis::ThreadPool::cleanup();
            return 0;
        }
    }

    Voraxis::UCI::loop();

    Voraxis::ThreadPool::cleanup();
    return 0;
}
