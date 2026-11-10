#include "board.h"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace Voraxis {

static uint64_t zobristPieces[12][64];
static uint64_t zobristSide;
static uint64_t zobristCastling[16];
static uint64_t zobristEP[65];
static bool zobristInitialized = false;

const uint8_t castlingRightsMask[64] = {
    13, 15, 15, 15, 12, 15, 15, 14, // a1=13 (~2), e1=12 (~3), h1=14 (~1)
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11  // a8=7 (~8), e8=3 (~12), h8=11 (~4)
};

static uint64_t pseudoRandom64(uint64_t& state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

void Board::initZobrist() {
    if (zobristInitialized) return;
    uint64_t state = 1804289383ULL;
    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            zobristPieces[p][sq] = pseudoRandom64(state);
        }
    }
    zobristSide = pseudoRandom64(state);
    for (int c = 0; c < 16; ++c) {
        zobristCastling[c] = pseudoRandom64(state);
    }
    for (int sq = 0; sq < 65; ++sq) {
        zobristEP[sq] = pseudoRandom64(state);
    }
    zobristInitialized = true;
}

std::string Move::toUCI() const {
    if (from < 0 || from >= 64 || to < 0 || to >= 64) return "0000";
    std::string s;
    s += static_cast<char>('a' + squareFile(from));
    s += static_cast<char>('1' + squareRank(from));
    s += static_cast<char>('a' + squareFile(to));
    s += static_cast<char>('1' + squareRank(to));
    if (promotion != NO_PIECE) {
        int pt = pieceType(promotion);
        if (pt == QUEEN) s += 'q';
        else if (pt == ROOK) s += 'r';
        else if (pt == BISHOP) s += 'b';
        else if (pt == KNIGHT) s += 'n';
    }
    return s;
}

Move Move::fromUCI(const std::string& uciStr, const Board& board) {
    if (uciStr.length() < 4) return Move();
    int fFile = uciStr[0] - 'a';
    int fRank = uciStr[1] - '1';
    int tFile = uciStr[2] - 'a';
    int tRank = uciStr[3] - '1';
    if (fFile < 0 || fFile > 7 || fRank < 0 || fRank > 7 ||
        tFile < 0 || tFile > 7 || tRank < 0 || tRank > 7) {
        return Move();
    }
    int fromSq = makeSquare(fFile, fRank);
    int toSq = makeSquare(tFile, tRank);
    int p = board.pieceOnSq[fromSq];
    if (p == NO_PIECE) return Move();

    int cap = board.pieceOnSq[toSq];
    int prom = NO_PIECE;
    if (uciStr.length() >= 5) {
        char prChar = static_cast<char>(std::tolower(uciStr[4]));
        int side = board.sideToMove;
        if (prChar == 'q') prom = (side == WHITE) ? W_QUEEN : B_QUEEN;
        else if (prChar == 'r') prom = (side == WHITE) ? W_ROOK : B_ROOK;
        else if (prChar == 'b') prom = (side == WHITE) ? W_BISHOP : B_BISHOP;
        else if (prChar == 'n') prom = (side == WHITE) ? W_KNIGHT : B_KNIGHT;
    }

    bool castle = false;
    if (pieceType(p) == KING && std::abs(fFile - tFile) == 2) {
        castle = true;
    }

    bool ep = false;
    if (pieceType(p) == PAWN && toSq == board.epSquare && fFile != tFile) {
        ep = true;
        cap = (board.sideToMove == WHITE) ? B_PAWN : W_PAWN;
    }

    bool dpp = false;
    if (pieceType(p) == PAWN && std::abs(tRank - fRank) == 2) {
        dpp = true;
    }

    return Move(fromSq, toSq, p, cap, prom, castle, ep, dpp);
}

Board::Board() {
    initZobrist();
    reset();
}

void Board::clear() {
    for (int i = 0; i < 12; ++i) pieceBB[i] = 0ULL;
    for (int i = 0; i < 3; ++i) occBB[i] = 0ULL;
    for (int i = 0; i < 64; ++i) pieceOnSq[i] = NO_PIECE;
    sideToMove = WHITE;
    castlingRights = NO_CASTLING;
    epSquare = SQ_NONE;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    zobristKey = 0ULL;
    history.clear();
    repetitionHistory.clear();
}

void Board::placePiece(int piece, int sq) {
    uint64_t mask = 1ULL << sq;
    pieceBB[piece] |= mask;
    occBB[pieceColor(piece)] |= mask;
    occBB[BOTH] |= mask;
    pieceOnSq[sq] = piece;
}

void Board::removePiece(int piece, int sq) {
    uint64_t mask = ~(1ULL << sq);
    pieceBB[piece] &= mask;
    occBB[pieceColor(piece)] &= mask;
    occBB[BOTH] &= mask;
    pieceOnSq[sq] = NO_PIECE;
}

void Board::reset() {
    loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

uint64_t Board::computeZobrist() const {
    uint64_t key = 0ULL;
    for (int sq = 0; sq < 64; ++sq) {
        int p = pieceOnSq[sq];
        if (p != NO_PIECE) {
            key ^= zobristPieces[p][sq];
        }
    }
    if (sideToMove == BLACK) {
        key ^= zobristSide;
    }
    key ^= zobristCastling[castlingRights];
    int epIdx = (epSquare != SQ_NONE) ? epSquare : 64;
    key ^= zobristEP[epIdx];
    return key;
}

bool Board::loadFEN(const std::string& fen) {
    clear();
    std::istringstream iss(fen);
    std::string piecePlacement, side, castling, ep;
    int halfmove = 0, fullmove = 1;

    if (!(iss >> piecePlacement)) return false;
    iss >> side >> castling >> ep >> halfmove >> fullmove;

    int rank = 7;
    int file = 0;
    for (char c : piecePlacement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (std::isdigit(c)) {
            file += (c - '0');
        } else {
            int sq = makeSquare(file, rank);
            int piece = NO_PIECE;
            switch (c) {
                case 'P': piece = W_PAWN; break;
                case 'N': piece = W_KNIGHT; break;
                case 'B': piece = W_BISHOP; break;
                case 'R': piece = W_ROOK; break;
                case 'Q': piece = W_QUEEN; break;
                case 'K': piece = W_KING; break;
                case 'p': piece = B_PAWN; break;
                case 'n': piece = B_KNIGHT; break;
                case 'b': piece = B_BISHOP; break;
                case 'r': piece = B_ROOK; break;
                case 'q': piece = B_QUEEN; break;
                case 'k': piece = B_KING; break;
                default: break;
            }
            if (piece != NO_PIECE) {
                placePiece(piece, sq);
            }
            file++;
        }
    }

    sideToMove = (side == "b" || side == "B") ? BLACK : WHITE;

    castlingRights = NO_CASTLING;
    for (char c : castling) {
        if (c == 'K') castlingRights |= WHITE_OO;
        else if (c == 'Q') castlingRights |= WHITE_OOO;
        else if (c == 'k') castlingRights |= BLACK_OO;
        else if (c == 'q') castlingRights |= BLACK_OOO;
    }

    if (ep != "-" && ep.length() >= 2) {
        int epF = ep[0] - 'a';
        int epR = ep[1] - '1';
        if (epF >= 0 && epF <= 7 && epR >= 0 && epR <= 7) {
            epSquare = makeSquare(epF, epR);
        } else {
            epSquare = SQ_NONE;
        }
    } else {
        epSquare = SQ_NONE;
    }

    halfmoveClock = halfmove;
    fullmoveNumber = (fullmove > 0) ? fullmove : 1;

    zobristKey = computeZobrist();
    repetitionHistory.push_back(zobristKey);
    return true;
}

std::string Board::getFEN() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            int sq = makeSquare(file, rank);
            int p = pieceOnSq[sq];
            if (p == NO_PIECE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                const char pieceChars[] = "PNBRQKpnbrqk";
                fen += pieceChars[p];
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (rank > 0) fen += '/';
    }

    fen += (sideToMove == WHITE) ? " w " : " b ";

    std::string castling;
    if (castlingRights & WHITE_OO) castling += 'K';
    if (castlingRights & WHITE_OOO) castling += 'Q';
    if (castlingRights & BLACK_OO) castling += 'k';
    if (castlingRights & BLACK_OOO) castling += 'q';
    if (castling.empty()) castling = "-";
    fen += castling + " ";

    if (epSquare != SQ_NONE) {
        fen += static_cast<char>('a' + squareFile(epSquare));
        fen += static_cast<char>('1' + squareRank(epSquare));
    } else {
        fen += "-";
    }

    fen += " " + std::to_string(halfmoveClock) + " " + std::to_string(fullmoveNumber);
    return fen;
}

void Board::print() const {
    std::cout << "\n";
    const char pieceChars[] = "PNBRQKpnbrqk.";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " ";
        for (int file = 0; file < 8; ++file) {
            int sq = makeSquare(file, rank);
            int p = pieceOnSq[sq];
            std::cout << (p == NO_PIECE ? '.' : pieceChars[p]) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n  a b c d e f g h\n\n";
    std::cout << "Side to move : " << (sideToMove == WHITE ? "White" : "Black") << "\n";
    std::cout << "Castling     : ";
    if (castlingRights & WHITE_OO) std::cout << "K";
    if (castlingRights & WHITE_OOO) std::cout << "Q";
    if (castlingRights & BLACK_OO) std::cout << "k";
    if (castlingRights & BLACK_OOO) std::cout << "q";
    if (castlingRights == 0) std::cout << "-";
    std::cout << "\n";
    std::cout << "En passant   : ";
    if (epSquare != SQ_NONE) {
        std::cout << static_cast<char>('a' + squareFile(epSquare))
                  << static_cast<char>('1' + squareRank(epSquare));
    } else {
        std::cout << "-";
    }
    std::cout << "\n";
    std::cout << "Halfmove clock: " << halfmoveClock << "\n";
    std::cout << "Fullmove number: " << fullmoveNumber << "\n";
    std::cout << "FEN: " << getFEN() << "\n\n";
}

int Board::getKingSquare(int color) const {
    int kingPiece = (color == WHITE) ? W_KING : B_KING;
    uint64_t bb = pieceBB[kingPiece];
    if (bb) return __builtin_ctzll(bb);
    return SQ_NONE;
}

void Board::applyMove(const Move& move) {
    UndoInfo undo;
    undo.move = move;
    undo.capturedPiece = move.captured;
    undo.castlingRights = castlingRights;
    undo.epSquare = epSquare;
    undo.halfmoveClock = halfmoveClock;
    undo.zobristKey = zobristKey;
    history.push_back(undo);

    if (pieceType(move.piece) == PAWN || move.captured != NO_PIECE) {
        halfmoveClock = 0;
    } else {
        halfmoveClock++;
    }

    castlingRights &= (castlingRightsMask[move.from] & castlingRightsMask[move.to]);

    if (move.isDoublePawnPush) {
        epSquare = (move.from + move.to) / 2;
    } else {
        epSquare = SQ_NONE;
    }

    if (move.captured != NO_PIECE) {
        if (move.isEnPassant) {
            int capSq = (sideToMove == WHITE) ? (move.to - 8) : (move.to + 8);
            removePiece(move.captured, capSq);
        } else {
            removePiece(move.captured, move.to);
        }
    }

    removePiece(move.piece, move.from);

    if (move.promotion != NO_PIECE) {
        placePiece(move.promotion, move.to);
    } else {
        placePiece(move.piece, move.to);
    }

    if (move.isCastling) {
        if (move.to == G1) {
            removePiece(W_ROOK, H1);
            placePiece(W_ROOK, F1);
        } else if (move.to == C1) {
            removePiece(W_ROOK, A1);
            placePiece(W_ROOK, D1);
        } else if (move.to == G8) {
            removePiece(B_ROOK, H8);
            placePiece(B_ROOK, F8);
        } else if (move.to == C8) {
            removePiece(B_ROOK, A8);
            placePiece(B_ROOK, D8);
        }
    }

    if (sideToMove == BLACK) {
        fullmoveNumber++;
    }
    sideToMove ^= 1;

    zobristKey = computeZobrist();
    repetitionHistory.push_back(zobristKey);
}

void Board::undoMove(const Move& move) {
    if (history.empty()) return;
    const UndoInfo undo = history.back();
    history.pop_back();
    repetitionHistory.pop_back();

    if (sideToMove == WHITE) {
        fullmoveNumber--;
    }
    sideToMove ^= 1;

    if (move.isCastling) {
        if (move.to == G1) {
            removePiece(W_ROOK, F1);
            placePiece(W_ROOK, H1);
        } else if (move.to == C1) {
            removePiece(W_ROOK, D1);
            placePiece(W_ROOK, A1);
        } else if (move.to == G8) {
            removePiece(B_ROOK, F8);
            placePiece(B_ROOK, H8);
        } else if (move.to == C8) {
            removePiece(B_ROOK, D8);
            placePiece(B_ROOK, A8);
        }
    }

    if (move.promotion != NO_PIECE) {
        removePiece(move.promotion, move.to);
    } else {
        removePiece(move.piece, move.to);
    }

    placePiece(move.piece, move.from);

    if (move.captured != NO_PIECE) {
        if (move.isEnPassant) {
            int capSq = (sideToMove == WHITE) ? (move.to - 8) : (move.to + 8);
            placePiece(undo.capturedPiece, capSq);
        } else {
            placePiece(undo.capturedPiece, move.to);
        }
    }

    castlingRights = undo.castlingRights;
    epSquare = undo.epSquare;
    halfmoveClock = undo.halfmoveClock;
    zobristKey = undo.zobristKey;
}

void Board::makeNullMove() {
    UndoInfo undo;
    undo.move = Move();
    undo.capturedPiece = NO_PIECE;
    undo.castlingRights = castlingRights;
    undo.epSquare = epSquare;
    undo.halfmoveClock = halfmoveClock;
    undo.zobristKey = zobristKey;
    history.push_back(undo);

    epSquare = SQ_NONE;
    sideToMove ^= 1;
    zobristKey = computeZobrist();
    repetitionHistory.push_back(zobristKey);
}

void Board::undoNullMove() {
    if (history.empty()) return;
    const UndoInfo undo = history.back();
    history.pop_back();
    repetitionHistory.pop_back();

    sideToMove ^= 1;
    epSquare = undo.epSquare;
    castlingRights = undo.castlingRights;
    halfmoveClock = undo.halfmoveClock;
    zobristKey = undo.zobristKey;
}

bool Board::hasNonPawnMaterial(int color) const {
    if (color == WHITE) {
        return (pieceBB[W_KNIGHT] | pieceBB[W_BISHOP] | pieceBB[W_ROOK] | pieceBB[W_QUEEN]) != 0ULL;
    } else {
        return (pieceBB[B_KNIGHT] | pieceBB[B_BISHOP] | pieceBB[B_ROOK] | pieceBB[B_QUEEN]) != 0ULL;
    }
}

bool Board::isRepetition(int ply) const {
    int count = 0;
    for (uint64_t key : repetitionHistory) {
        if (key == zobristKey) {
            count++;
            if (ply > 0 && count >= 2) return true;
            if (count >= 3) return true;
        }
    }
    return false;
}

bool Board::isThreefoldRepetition() const {
    return isRepetition(0);
}

bool Board::isInsufficientMaterial() const {
    if (pieceBB[W_PAWN] || pieceBB[B_PAWN] ||
        pieceBB[W_ROOK] || pieceBB[B_ROOK] ||
        pieceBB[W_QUEEN] || pieceBB[B_QUEEN]) {
        return false;
    }

    int wKnights = __builtin_popcountll(pieceBB[W_KNIGHT]);
    int bKnights = __builtin_popcountll(pieceBB[B_KNIGHT]);
    int wBishops = __builtin_popcountll(pieceBB[W_BISHOP]);
    int bBishops = __builtin_popcountll(pieceBB[B_BISHOP]);

    int whiteMinors = wKnights + wBishops;
    int blackMinors = bKnights + bBishops;

    // King vs King
    if (whiteMinors == 0 && blackMinors == 0) return true;

    // King + minor vs King
    if ((whiteMinors == 1 && blackMinors == 0) || (whiteMinors == 0 && blackMinors == 1)) {
        return true;
    }

    // King + Bishop vs King + Bishop of same color
    if (whiteMinors == 1 && blackMinors == 1 && wBishops == 1 && bBishops == 1) {
        int wBsq = __builtin_ctzll(pieceBB[W_BISHOP]);
        int bBsq = __builtin_ctzll(pieceBB[B_BISHOP]);
        bool wLight = ((squareRank(wBsq) + squareFile(wBsq)) % 2) != 0;
        bool bLight = ((squareRank(bBsq) + squareFile(bBsq)) % 2) != 0;
        if (wLight == bLight) return true;
    }

    return false;
}

} // namespace Voraxis
