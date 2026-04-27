#include "Board.h"
#include <cstring>
#include <sstream>
#include <random>
#include <stdexcept>

// --- Zobrist tables ---
static uint64_t zPiece[2][6][64];
static uint64_t zSide;
static uint64_t zCastle[16];
static uint64_t zEP[8];
static bool     zInit = false;

void Board::initZobrist() {
    if (zInit) return;
    std::mt19937_64 rng(0xDEADBEEFCAFEBABEULL);
    for (int c=0;c<2;c++)
        for (int p=0;p<6;p++)
            for (int s=0;s<64;s++)
                zPiece[c][p][s]=rng();
    zSide=rng();
    for (int i=0;i<16;i++) zCastle[i]=rng();
    for (int i=0;i<8;i++)  zEP[i]=rng();
    zInit=true;
}

// --- Board construction ---
Board::Board() {
    initZobrist();
    memset(pieces,0,sizeof(pieces));
    memset(occupied,0,sizeof(occupied));
    allOccupied=0;
    memset(pieceOn,255,sizeof(pieceOn));
    memset(colorOn,255,sizeof(colorOn));
    sideToMove=WHITE;
    castlingRights=0;
    enPassantSquare=-1;
    halfmoveClock=0;
    fullmoveNumber=1;
    hash=0;
    undoTop=0;
}

void Board::putPiece(Color c, PieceType pt, int sq) {
    uint64_t b=bit(sq);
    pieces[c][pt]|=b;
    occupied[c]  |=b;
    allOccupied  |=b;
    pieceOn[sq]=(uint8_t)pt;
    colorOn[sq]=(uint8_t)c;
    hash^=zPiece[c][pt][sq];
}

void Board::removePiece(Color c, PieceType pt, int sq) {
    uint64_t b=bit(sq);
    pieces[c][pt]&=~b;
    occupied[c]  &=~b;
    allOccupied  &=~b;
    pieceOn[sq]=255;
    colorOn[sq]=255;
    hash^=zPiece[c][pt][sq];
}

// --- FEN parsing ---
bool Board::parseFEN(const std::string& fen) {
    // Clear board
    memset(pieces,0,sizeof(pieces));
    memset(occupied,0,sizeof(occupied));
    allOccupied=0;
    memset(pieceOn,255,sizeof(pieceOn));
    memset(colorOn,255,sizeof(colorOn));
    hash=0; undoTop=0;

    std::istringstream ss(fen);
    std::string placement,side,castle,ep;
    int halfmove=0,fullmove=1;
    ss>>placement>>side>>castle>>ep>>halfmove>>fullmove;

    // Piece placement: rank 8 first
    int rank8=7,file8=0;
    for (char ch : placement) {
        if (ch=='/') { rank8--; file8=0; }
        else if (ch>='1'&&ch<='8') { file8+=ch-'0'; }
        else {
            Color c = (ch>='a'&&ch<='z') ? BLACK : WHITE;
            char lower = ch|0x20;
            PieceType pt;
            switch(lower) {
                case 'p': pt=PAWN;   break;
                case 'n': pt=KNIGHT; break;
                case 'b': pt=BISHOP; break;
                case 'r': pt=ROOK;   break;
                case 'q': pt=QUEEN;  break;
                case 'k': pt=KING;   break;
                default:  return false;
            }
            putPiece(c,pt,sq(file8,rank8));
            file8++;
        }
    }

    sideToMove = (side=="w") ? WHITE : BLACK;
    if (sideToMove==BLACK) hash^=zSide;

    castlingRights=0;
    for (char ch : castle) {
        if (ch=='K') castlingRights|=WK;
        if (ch=='Q') castlingRights|=WQ;
        if (ch=='k') castlingRights|=BK;
        if (ch=='q') castlingRights|=BQ;
    }
    hash^=zCastle[castlingRights];

    enPassantSquare=-1;
    if (ep!="-") {
        int f=ep[0]-'a', r=ep[1]-'1';
        enPassantSquare=sq(f,r);
        hash^=zEP[f];
    }

    halfmoveClock=halfmove;
    fullmoveNumber=fullmove;
    return true;
}

void Board::setStartPosition() {
    parseFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

int Board::kingSquare(Color c) const {
    return lsb(pieces[c][KING]);
}

bool Board::isThreefoldRepetition() const {
    int matches = 1; // current position
    for (int i = 0; i < undoTop; i++) {
        if (undoStack[i].hash == hash) {
            matches++;
            if (matches >= 3) return true;
        }
    }
    return false;
}

bool Board::isFiftyMoveDraw() const {
    return halfmoveClock >= 100;
}

bool Board::isInsufficientMaterial() const {
    if (pieces[WHITE][PAWN] || pieces[BLACK][PAWN] ||
        pieces[WHITE][ROOK] || pieces[BLACK][ROOK] ||
        pieces[WHITE][QUEEN] || pieces[BLACK][QUEEN]) {
        return false;
    }

    const int wKnights = popcount(pieces[WHITE][KNIGHT]);
    const int bKnights = popcount(pieces[BLACK][KNIGHT]);
    const int wBishops = popcount(pieces[WHITE][BISHOP]);
    const int bBishops = popcount(pieces[BLACK][BISHOP]);
    const int totalMinors = wKnights + bKnights + wBishops + bBishops;

    if (totalMinors == 0) return true; // K vs K
    if (totalMinors == 1) return true; // K+N/B vs K

    // K+B vs K+B with bishops on same square color
    if (wKnights == 0 && bKnights == 0 && wBishops == 1 && bBishops == 1) {
        int wSq = lsb(pieces[WHITE][BISHOP]);
        int bSq = lsb(pieces[BLACK][BISHOP]);
        bool wDark = ((file(wSq) + rank(wSq)) & 1) == 0;
        bool bDark = ((file(bSq) + rank(bSq)) & 1) == 0;
        return wDark == bDark;
    }

    return false;
}

bool Board::isDraw() const {
    return isFiftyMoveDraw() || isThreefoldRepetition() || isInsufficientMaterial();
}

// --- Make/Unmake ---
void Board::makeMove(const Move& m) {
    UndoInfo& u = undoStack[undoTop++];
    u.castlingRights  = castlingRights;
    u.enPassantSquare = enPassantSquare;
    u.halfmoveClock   = halfmoveClock;
    u.hash            = hash;
    u.captured        = 255;
    u.capturedColor   = 255;

    Color us   = sideToMove;
    Color them = (Color)(1-us);

    // Remove EP and castle from hash before modifying
    hash ^= zCastle[castlingRights];
    if (enPassantSquare!=-1) hash^=zEP[::file(enPassantSquare)];

    enPassantSquare=-1;
    halfmoveClock++;

    PieceType movingPt = (PieceType)m.piece;

    if (m.enPassant) {
        // Remove captured pawn
        int capSq = (us==WHITE) ? m.to-8 : m.to+8;
        u.captured=(uint8_t)PAWN;
        u.capturedColor=(uint8_t)them;
        removePiece(them,PAWN,capSq);
        removePiece(us,movingPt,m.from);
        putPiece(us,movingPt,m.to);
        halfmoveClock=0;
    } else if (m.castling) {
        // Move king
        removePiece(us,KING,m.from);
        putPiece(us,KING,m.to);
        // Move rook
        int rookFrom,rookTo;
        if (m.to==sq(6,us==WHITE?0:7)) { rookFrom=sq(7,us==WHITE?0:7); rookTo=sq(5,us==WHITE?0:7); }
        else                            { rookFrom=sq(0,us==WHITE?0:7); rookTo=sq(3,us==WHITE?0:7); }
        removePiece(us,ROOK,rookFrom);
        putPiece(us,ROOK,rookTo);
    } else {
        // Normal capture
        if (m.captured!=NO_PIECE) {
            u.captured=(uint8_t)m.captured;
            u.capturedColor=(uint8_t)them;
            removePiece(them,(PieceType)m.captured,m.to);
            halfmoveClock=0;
        }
        removePiece(us,movingPt,m.from);
        PieceType landing = (m.promoted!=NO_PIECE) ? (PieceType)m.promoted : movingPt;
        putPiece(us,landing,m.to);
        if (movingPt==PAWN) {
            halfmoveClock=0;
            // Set EP square for double push
            if (abs(m.to-m.from)==16) {
                enPassantSquare=(m.from+m.to)/2;
                hash^=zEP[::file(enPassantSquare)];
            }
        }
    }

    // Update castling rights
    const int castleMask[64]={
        ~WQ,15,15,15,~(WK|WQ),15,15,~WK,
        15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,
        ~BQ,15,15,15,~(BK|BQ),15,15,~BK
    };
    castlingRights &= castleMask[m.from] & castleMask[m.to];
    hash ^= zCastle[castlingRights];

    sideToMove=them;
    hash^=zSide;
    if (us==BLACK) fullmoveNumber++;
}

void Board::unmakeMove(const Move& m) {
    Color them = sideToMove;            // side that just moved
    Color us   = (Color)(1-them);       // side we're restoring

    if (them==WHITE) fullmoveNumber--;

    if (m.castling) {
        putPiece(us,KING,m.from);
        removePiece(us,KING,m.to);
        int rookFrom,rookTo;
        if (m.to==sq(6,us==WHITE?0:7)) { rookFrom=sq(7,us==WHITE?0:7); rookTo=sq(5,us==WHITE?0:7); }
        else                            { rookFrom=sq(0,us==WHITE?0:7); rookTo=sq(3,us==WHITE?0:7); }
        putPiece(us,ROOK,rookFrom);
        removePiece(us,ROOK,rookTo);
    } else if (m.enPassant) {
        int capSq = (us==WHITE) ? m.to-8 : m.to+8;
        removePiece(us,(PieceType)m.piece,m.to);
        putPiece(us,(PieceType)m.piece,m.from);
        putPiece(them,PAWN,capSq);
    } else {
        PieceType landing=(m.promoted!=NO_PIECE)?(PieceType)m.promoted:(PieceType)m.piece;
        removePiece(us,landing,m.to);
        putPiece(us,(PieceType)m.piece,m.from);
        if (m.captured!=NO_PIECE)
            putPiece(them,(PieceType)m.captured,m.to);
    }

    UndoInfo& u=undoStack[--undoTop];
    castlingRights  =u.castlingRights;
    enPassantSquare =u.enPassantSquare;
    halfmoveClock   =u.halfmoveClock;
    hash            =u.hash;
    sideToMove=us;
}

void Board::makeNullMove() {
    UndoInfo& u = undoStack[undoTop++];
    u.castlingRights  = castlingRights;
    u.enPassantSquare = enPassantSquare;
    u.halfmoveClock   = halfmoveClock;
    u.hash            = hash;
    u.captured        = 255;
    u.capturedColor   = 255;
    if (enPassantSquare != -1) { hash ^= zEP[::file(enPassantSquare)]; enPassantSquare = -1; }
    sideToMove = (Color)(1 - sideToMove);
    hash ^= zSide;
    halfmoveClock++;
}

void Board::unmakeNullMove() {
    sideToMove = (Color)(1 - sideToMove);
    UndoInfo& u = undoStack[--undoTop];
    castlingRights  = u.castlingRights;
    enPassantSquare = u.enPassantSquare;
    halfmoveClock   = u.halfmoveClock;
    hash            = u.hash;
}
