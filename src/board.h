#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <iostream>

namespace Voraxis {

enum Color {
    WHITE = 0,
    BLACK = 1,
    BOTH = 2
};

enum PieceType {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6
};

enum Piece {
    W_PAWN = 0, W_KNIGHT = 1, W_BISHOP = 2, W_ROOK = 3, W_QUEEN = 4, W_KING = 5,
    B_PAWN = 6, B_KNIGHT = 7, B_BISHOP = 8, B_ROOK = 9, B_QUEEN = 10, B_KING = 11,
    NO_PIECE = 12
};

enum Square {
    A1 = 0, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = -1
};

enum CastlingRights {
    NO_CASTLING = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    ALL_CASTLING = 15
};

struct Move {
    int from = SQ_NONE;
    int to = SQ_NONE;
    int piece = NO_PIECE;
    int captured = NO_PIECE;
    int promotion = NO_PIECE;
    bool isCastling = false;
    bool isEnPassant = false;
    bool isDoublePawnPush = false;

    Move() = default;
    Move(int f, int t, int p, int cap = NO_PIECE, int prom = NO_PIECE,
         bool castle = false, bool ep = false, bool dpp = false)
        : from(f), to(t), piece(p), captured(cap), promotion(prom),
          isCastling(castle), isEnPassant(ep), isDoublePawnPush(dpp) {}

    bool operator==(const Move& other) const {
        return from == other.from && to == other.to && promotion == other.promotion;
    }
    bool operator!=(const Move& other) const {
        return !(*this == other);
    }

    std::string toUCI() const;
    static Move fromUCI(const std::string& uciStr, const class Board& board);
};

struct UndoInfo {
    Move move;
    int capturedPiece = NO_PIECE;
    uint8_t castlingRights = 0;
    int epSquare = SQ_NONE;
    int halfmoveClock = 0;
    uint64_t zobristKey = 0ULL;
};

class Board {
public:
    uint64_t pieceBB[12]{};
    uint64_t occBB[3]{};
    int pieceOnSq[64]{};

    int sideToMove = WHITE;
    uint8_t castlingRights = ALL_CASTLING;
    int epSquare = SQ_NONE;
    int halfmoveClock = 0;
    int fullmoveNumber = 1;
    uint64_t zobristKey = 0ULL;

    std::vector<UndoInfo> history;
    std::vector<uint64_t> repetitionHistory;

    Board();
    void reset();
    bool loadFEN(const std::string& fen);
    std::string getFEN() const;
    void print() const;

    void applyMove(const Move& move);
    void undoMove(const Move& move);

    bool isFiftyMoveDraw() const { return halfmoveClock >= 100; }
    bool isThreefoldRepetition() const;
    bool isInsufficientMaterial() const;

    int getSideToMove() const { return sideToMove; }
    int getKingSquare(int color) const;

    static void initZobrist();
    uint64_t computeZobrist() const;

private:
    void clear();
    void placePiece(int piece, int sq);
    void removePiece(int piece, int sq);
};

// Global castling update lookup array
extern const uint8_t castlingRightsMask[64];

inline int makeSquare(int file, int rank) { return rank * 8 + file; }
inline int squareFile(int sq) { return sq & 7; }
inline int squareRank(int sq) { return sq >> 3; }
inline int pieceColor(int piece) { return piece / 6; }
inline int pieceType(int piece) { return piece % 6; }

} // namespace Voraxis

#endif // BOARD_H
