#pragma once
#include <cstdint>
#include <string>
#include <array>

// Piece indices: 0=Pawn,1=Knight,2=Bishop,3=Rook,4=Queen,5=King
// Color: 0=White, 1=Black
enum PieceType { PAWN=0, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE };
enum Color { WHITE=0, BLACK=1, NO_COLOR };

// Castling rights bitmask
enum CastlingRights { WK=1, WQ=2, BK=4, BQ=8 };

struct Move {
    uint8_t from;
    uint8_t to;
    uint8_t piece;       // moving piece type
    uint8_t captured;    // captured piece type (NO_PIECE if none)
    uint8_t promoted;    // promotion piece type (NO_PIECE if none)
    bool    enPassant;
    bool    castling;

    Move() : from(0),to(0),piece(NO_PIECE),captured(NO_PIECE),
             promoted(NO_PIECE),enPassant(false),castling(false) {}
    Move(uint8_t f,uint8_t t,uint8_t p,uint8_t cap=NO_PIECE,
         uint8_t promo=NO_PIECE,bool ep=false,bool cast=false)
        : from(f),to(t),piece(p),captured(cap),
          promoted(promo),enPassant(ep),castling(cast) {}
};

struct Board {
    // 12 bitboards: [color][piece_type]
    uint64_t pieces[2][6];

    // Occupancy helpers (derived, kept in sync)
    uint64_t occupied[2]; // occupied[c] = all pieces of color c
    uint64_t allOccupied;

    // Board state
    Color    sideToMove;
    int      castlingRights; // bitmask of CastlingRights
    int      enPassantSquare; // -1 if none, else 0-63
    int      halfmoveClock;
    int      fullmoveNumber;

    // Piece map: square -> (color, piece_type) for fast lookup
    uint8_t  pieceOn[64];  // encodes color*6+type, 255=empty
    uint8_t  colorOn[64];  // NO_COLOR=255 if empty

    Board();
    void     setStartPosition();
    bool     parseFEN(const std::string& fen);
    void     makeMove(const Move& m);
    void     unmakeMove(const Move& m);
    void     makeNullMove();
    void     unmakeNullMove();

    // Low-level helpers
    void     putPiece(Color c, PieceType pt, int sq);
    void     removePiece(Color c, PieceType pt, int sq);

    // Returns the king square for the given color
    int      kingSquare(Color c) const;

    // Zobrist hash of current position
    uint64_t hash;
    void     updateHash(Color c, PieceType pt, int sq);
    void     initZobrist();
    bool     isThreefoldRepetition() const;
    bool     isFiftyMoveDraw() const;
    bool     isInsufficientMaterial() const;
    bool     isDraw() const;

    // Undo state stack
    struct UndoInfo {
        int      castlingRights;
        int      enPassantSquare;
        int      halfmoveClock;
        uint64_t hash;
        uint8_t  captured;
        uint8_t  capturedColor;
    };
    static constexpr int MAX_DEPTH = 128;
    UndoInfo undoStack[MAX_DEPTH];
    int      undoTop;

    std::string toFEN() const;
};

// Bit manipulation utilities
inline int  lsb(uint64_t b)       { return __builtin_ctzll(b); }
inline int  popcount(uint64_t b)  { return __builtin_popcountll(b); }
inline int  popLSB(uint64_t& b)   { int sq=lsb(b); b&=b-1; return sq; }
inline uint64_t bit(int sq)       { return 1ULL<<sq; }

// Square naming helpers
inline int  sq(int file, int rank) { return rank*8+file; }
inline int  file(int sq)           { return sq&7; }
inline int  rank(int sq)           { return sq>>3; }
