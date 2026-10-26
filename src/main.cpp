#include <iostream>
#include <string>
#include <array>

class Board {
public:
    std::array<char, 64> squares{};

    Board() {
        squares.fill('.');

        const std::string backRank = "rnbqkbnr";

        for (int file = 0; file < 8; ++file) {
            squares[file] = backRank[file];
            squares[8 + file] = 'p';

            squares[48 + file] = 'P';
            squares[56 + file] = backRank[file] - 'a' + 'A';
        }
    }

    void print() const {
        for (int rank = 0; rank < 8; ++rank) {
            std::cout << 8 - rank << " ";

            for (int file = 0; file < 8; ++file) {
                std::cout << squares[rank * 8 + file] << ' ';
            }

            std::cout << '\n';
        }

        std::cout << "\n  a b c d e f g h\n";
    }
};

int main() {
    Board board;
    board.print();

    return 0;
}