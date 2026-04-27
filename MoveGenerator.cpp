#include "MoveGenerator.h"
#include <cstring>

// ---- Attack table init ----
MoveGenerator::MoveGenerator() { initAttackTables(); }

void MoveGenerator::initAttackTables() {
    for (int s=0;s<64;s++) {
        int r=rank(s), f=::file(s);
        knightAttacks[s]=0;
        kingAttacks[s]=0;
        pawnAttacks[WHITE][s]=0;
        pawnAttacks[BLACK][s]=0;

        // Knight
        int kdx[]={-2,-2,-1,-1,1,1,2,2};
        int kdy[]={-1,1,-2,2,-2,2,-1,1};
        for (int i=0;i<8;i++) {
            int nr=r+kdy[i], nf=f+kdx[i];
            if (nr>=0&&nr<8&&nf>=0&&nf<8)
                knightAttacks[s]|=bit(sq(nf,nr));
        }
        // King
        for (int dr=-1;dr<=1;dr++) for (int df=-1;df<=1;df++) {
            if (!dr&&!df) continue;
            int nr=r+dr, nf=f+df;
            if (nr>=0&&nr<8&&nf>=0&&nf<8)
                kingAttacks[s]|=bit(sq(nf,nr));
        }
        // Pawn attacks
        if (r+1<8) {
            if (f-1>=0) pawnAttacks[WHITE][s]|=bit(sq(f-1,r+1));
            if (f+1<8)  pawnAttacks[WHITE][s]|=bit(sq(f+1,r+1));
        }
        if (r-1>=0) {
            if (f-1>=0) pawnAttacks[BLACK][s]|=bit(sq(f-1,r-1));
            if (f+1<8)  pawnAttacks[BLACK][s]|=bit(sq(f+1,r-1));
        }
    }
}

// ---- Sliding attacks (classical ray approach) ----
static uint64_t rayAttack(int sq, uint64_t occ, int df, int dr) {
    uint64_t result=0;
    int f=::file(sq), r=rank(sq);
    int nf=f+df, nr=r+dr;
    while (nf>=0&&nf<8&&nr>=0&&nr<8) {
        int ns=::sq(nf,nr);
        result|=bit(ns);
        if (occ&bit(ns)) break; // blocked
        nf+=df; nr+=dr;
    }
    return result;
}

uint64_t MoveGenerator::rookAttacks(int s, uint64_t occ) const {
    return rayAttack(s,occ, 1,0)|rayAttack(s,occ,-1,0)
          |rayAttack(s,occ,0, 1)|rayAttack(s,occ,0,-1);
}
uint64_t MoveGenerator::bishopAttacks(int s, uint64_t occ) const {
    return rayAttack(s,occ, 1, 1)|rayAttack(s,occ,-1, 1)
          |rayAttack(s,occ, 1,-1)|rayAttack(s,occ,-1,-1);
}
uint64_t MoveGenerator::queenAttacks(int s, uint64_t occ) const {
    return rookAttacks(s,occ)|bishopAttacks(s,occ);
}

// ---- Square attacked? ----
bool MoveGenerator::isSquareAttacked(const Board& b, int s, Color by) const {
    uint64_t occ=b.allOccupied;
    Color atk=by;
    if (knightAttacks[s] & b.pieces[atk][KNIGHT]) return true;
    if (kingAttacks[s]   & b.pieces[atk][KING])   return true;
    if (pawnAttacks[1-atk][s] & b.pieces[atk][PAWN]) return true; // attackers see FROM defender side
    if (rookAttacks(s,occ)   & (b.pieces[atk][ROOK]|b.pieces[atk][QUEEN])) return true;
    if (bishopAttacks(s,occ) & (b.pieces[atk][BISHOP]|b.pieces[atk][QUEEN])) return true;
    return false;
}

bool MoveGenerator::isInCheck(const Board& b, Color c) const {
    int ks=b.kingSquare(c);
    return isSquareAttacked(b,ks,(Color)(1-c));
}

// ---- Pawn moves ----
void MoveGenerator::genPawnMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    Color them=(Color)(1-us);
    uint64_t pawns=b.pieces[us][PAWN];
    uint64_t occ=b.allOccupied;
    uint64_t enemy=b.occupied[them];

    int pushDir   = (us==WHITE) ? 8  : -8;
    int startRank = (us==WHITE) ? 1  : 6;
    int promoRank = (us==WHITE) ? 7  : 0;

    while (pawns) {
        int from=popLSB(pawns);
        int r=rank(from);

        // Single push
        int to1=from+pushDir;
        if (!capturesOnly && to1>=0 && to1<64 && !(occ&bit(to1))) {
            if (rank(to1)==promoRank) {
                for (int p=KNIGHT;p<=QUEEN;p++)
                    ml.push_back(Move(from,to1,PAWN,NO_PIECE,(PieceType)p));
            } else {
                ml.push_back(Move(from,to1,PAWN));
                // Double push
                if (r==startRank) {
                    int to2=from+2*pushDir;
                    if (!(occ&bit(to2)))
                        ml.push_back(Move(from,to2,PAWN));
                }
            }
        }

        // Captures
        uint64_t caps=pawnAttacks[us][from]&enemy;
        while (caps) {
            int to=popLSB(caps);
            PieceType cap=(PieceType)b.pieceOn[to];
            if (rank(to)==promoRank) {
                for (int p=KNIGHT;p<=QUEEN;p++)
                    ml.push_back(Move(from,to,PAWN,cap,(PieceType)p));
            } else {
                ml.push_back(Move(from,to,PAWN,cap));
            }
        }

        // En passant
        if (b.enPassantSquare!=-1) {
            if (pawnAttacks[us][from]&bit(b.enPassantSquare))
                ml.push_back(Move(from,b.enPassantSquare,PAWN,PAWN,NO_PIECE,true));
        }
    }
}

void MoveGenerator::genKnightMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    uint64_t knights=b.pieces[us][KNIGHT];
    uint64_t own=b.occupied[us];
    uint64_t enemy=b.occupied[1-us];
    while (knights) {
        int from=popLSB(knights);
        uint64_t atk=knightAttacks[from]&~own;
        if (capturesOnly) atk&=enemy;
        while (atk) {
            int to=popLSB(atk);
            PieceType cap=(b.colorOn[to]==(uint8_t)(1-us))?(PieceType)b.pieceOn[to]:NO_PIECE;
            ml.push_back(Move(from,to,KNIGHT,cap));
        }
    }
}

void MoveGenerator::genBishopMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    uint64_t bishops=b.pieces[us][BISHOP];
    uint64_t own=b.occupied[us], enemy=b.occupied[1-us];
    while (bishops) {
        int from=popLSB(bishops);
        uint64_t atk=bishopAttacks(from,b.allOccupied)&~own;
        if (capturesOnly) atk&=enemy;
        while (atk) {
            int to=popLSB(atk);
            PieceType cap=(b.colorOn[to]==(uint8_t)(1-us))?(PieceType)b.pieceOn[to]:NO_PIECE;
            ml.push_back(Move(from,to,BISHOP,cap));
        }
    }
}

void MoveGenerator::genRookMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    uint64_t rooks=b.pieces[us][ROOK];
    uint64_t own=b.occupied[us], enemy=b.occupied[1-us];
    while (rooks) {
        int from=popLSB(rooks);
        uint64_t atk=rookAttacks(from,b.allOccupied)&~own;
        if (capturesOnly) atk&=enemy;
        while (atk) {
            int to=popLSB(atk);
            PieceType cap=(b.colorOn[to]==(uint8_t)(1-us))?(PieceType)b.pieceOn[to]:NO_PIECE;
            ml.push_back(Move(from,to,ROOK,cap));
        }
    }
}

void MoveGenerator::genQueenMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    uint64_t queens=b.pieces[us][QUEEN];
    uint64_t own=b.occupied[us], enemy=b.occupied[1-us];
    while (queens) {
        int from=popLSB(queens);
        uint64_t atk=queenAttacks(from,b.allOccupied)&~own;
        if (capturesOnly) atk&=enemy;
        while (atk) {
            int to=popLSB(atk);
            PieceType cap=(b.colorOn[to]==(uint8_t)(1-us))?(PieceType)b.pieceOn[to]:NO_PIECE;
            ml.push_back(Move(from,to,QUEEN,cap));
        }
    }
}

void MoveGenerator::genKingMoves(const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const {
    int from=b.kingSquare(us);
    uint64_t own=b.occupied[us], enemy=b.occupied[1-us];
    uint64_t atk=kingAttacks[from]&~own;
    if (capturesOnly) atk&=enemy;
    while (atk) {
        int to=popLSB(atk);
        PieceType cap=(b.colorOn[to]==(uint8_t)(1-us))?(PieceType)b.pieceOn[to]:NO_PIECE;
        ml.push_back(Move(from,to,KING,cap));
    }
}

void MoveGenerator::genCastling(const Board& b, Color us, std::vector<Move>& ml) const {
    Color them=(Color)(1-us);
    int baseRank=(us==WHITE)?0:7;
    int kingFrom=sq(4,baseRank);

    if (isSquareAttacked(b,kingFrom,them)) return; // can't castle in check

    uint64_t occ=b.allOccupied;

    // Kingside
    if (b.castlingRights & (us==WHITE?WK:BK)) {
        int f5=sq(5,baseRank), f6=sq(6,baseRank);
        if (!(occ&bit(f5)) && !(occ&bit(f6))
            && !isSquareAttacked(b,f5,them)
            && !isSquareAttacked(b,f6,them))
        {
            ml.push_back(Move(kingFrom,f6,KING,NO_PIECE,NO_PIECE,false,true));
        }
    }
    // Queenside
    if (b.castlingRights & (us==WHITE?WQ:BQ)) {
        int f3=sq(3,baseRank), f2=sq(2,baseRank), f1=sq(1,baseRank);
        if (!(occ&bit(f3)) && !(occ&bit(f2)) && !(occ&bit(f1))
            && !isSquareAttacked(b,f3,them)
            && !isSquareAttacked(b,f2,them))
        {
            ml.push_back(Move(kingFrom,f2,KING,NO_PIECE,NO_PIECE,false,true));
        }
    }
}

// ---- Public generators ----
std::vector<Move> MoveGenerator::generateMoves(Board& board) {
    std::vector<Move> pseudo;
    pseudo.reserve(64);
    Color us=board.sideToMove;
    genPawnMoves  (board,us,pseudo,false);
    genKnightMoves(board,us,pseudo,false);
    genBishopMoves(board,us,pseudo,false);
    genRookMoves  (board,us,pseudo,false);
    genQueenMoves (board,us,pseudo,false);
    genKingMoves  (board,us,pseudo,false);
    genCastling   (board,us,pseudo);

    // Filter: keep only moves that don't leave king in check
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    for (auto& m : pseudo) {
        board.makeMove(m);
        if (!isInCheck(board,us))
            legal.push_back(m);
        board.unmakeMove(m);
    }
    return legal;
}

std::vector<Move> MoveGenerator::generateCaptures(Board& board) {
    std::vector<Move> pseudo;
    pseudo.reserve(16);
    Color us=board.sideToMove;
    genPawnMoves  (board,us,pseudo,true);
    genKnightMoves(board,us,pseudo,true);
    genBishopMoves(board,us,pseudo,true);
    genRookMoves  (board,us,pseudo,true);
    genQueenMoves (board,us,pseudo,true);
    genKingMoves  (board,us,pseudo,true);

    std::vector<Move> legal;
    for (auto& m : pseudo) {
        board.makeMove(m);
        if (!isInCheck(board,us))
            legal.push_back(m);
        board.unmakeMove(m);
    }
    return legal;
}
