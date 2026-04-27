#include "Evaluator.h"
#include <algorithm>
#include <cstring>

// ============================================================
//  Piece-Square Tables  (white perspective, rank 1 = index 0)
// ============================================================

const int Evaluator::MG_PST[6][64] = {
// PAWN
{ 0,  0,  0,  0,  0,  0,  0,  0,
  5, 10, 10,-20,-20, 10, 10,  5,
  5, -5,-10,  0,  0,-10, -5,  5,
  0,  0,  0, 20, 20,  0,  0,  0,
  5,  5, 10, 25, 25, 10,  5,  5,
 10, 10, 20, 30, 30, 20, 10, 10,
 50, 50, 50, 50, 50, 50, 50, 50,
  0,  0,  0,  0,  0,  0,  0,  0},
// KNIGHT
{-50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50},
// BISHOP
{-20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20},
// ROOK
{  0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0},
// QUEEN
{-20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -10,  5,  5,  5,  5,  5,  0,-10,
   0,  0,  5,  5,  5,  5,  0, -5,
  -5,  0,  5,  5,  5,  5,  0, -5,
 -10,  0,  5,  5,  5,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20},
// KING (middlegame)
{ 20, 30, 10,  0,  0, 10, 30, 20,
  20, 20,  0,  0,  0,  0, 20, 20,
 -10,-20,-20,-20,-20,-20,-20,-10,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30}
};

const int Evaluator::EG_KING_PST[64] = {
 -50,-40,-30,-20,-20,-30,-40,-50,
 -30,-20,-10,  0,  0,-10,-20,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-30,  0,  0,  0,  0,-30,-30,
 -50,-30,-30,-30,-30,-30,-30,-50
};

const int Evaluator::EG_PAWN_PST[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
  80, 80, 80, 80, 80, 80, 80, 80,
  50, 50, 50, 50, 50, 50, 50, 50,
  30, 30, 30, 30, 30, 30, 30, 30,
  20, 20, 20, 20, 20, 20, 20, 20,
  10, 10, 10, 10, 10, 10, 10, 10,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0
};

// ============================================================
//  Constructor
// ============================================================
Evaluator::Evaluator() {
    for (int s = 0; s < 64; s++) {
        knightAtk[s] = 0;
        int r = rank(s), f = ::file(s);
        const int dr[] = {-2,-2,-1,-1, 1, 1, 2, 2};
        const int df[] = {-1, 1,-2, 2,-2, 2,-1, 1};
        for (int i = 0; i < 8; i++) {
            int nr = r+dr[i], nf = f+df[i];
            if (nr>=0&&nr<8&&nf>=0&&nf<8) knightAtk[s]|=bit(sq(nf,nr));
        }
    }

    for (int f = 0; f < 8; f++) {
        fileMask[f] = 0x0101010101010101ULL << f;
        adjFileMask[f] = 0;
        if (f > 0) adjFileMask[f] |= fileMask[f-1];
        if (f < 7) adjFileMask[f] |= fileMask[f+1];
    }

    for (int s = 0; s < 64; s++) {
        int r = rank(s), f = ::file(s);
        passedMask[WHITE][s] = passedMask[BLACK][s] = 0;
        for (int rr = r+1; rr < 8; rr++) {
            if (f>0) passedMask[WHITE][s]|=bit(sq(f-1,rr));
            passedMask[WHITE][s]|=bit(sq(f,rr));
            if (f<7) passedMask[WHITE][s]|=bit(sq(f+1,rr));
        }
        for (int rr = r-1; rr >= 0; rr--) {
            if (f>0) passedMask[BLACK][s]|=bit(sq(f-1,rr));
            passedMask[BLACK][s]|=bit(sq(f,rr));
            if (f<7) passedMask[BLACK][s]|=bit(sq(f+1,rr));
        }
    }
}

// ============================================================
//  Sliding attacks
// ============================================================
uint64_t Evaluator::rayAtk(int sq, uint64_t occ, int df, int dr) {
    uint64_t res = 0;
    int f = ::file(sq), r = rank(sq);
    for (int nf=f+df,nr=r+dr; nf>=0&&nf<8&&nr>=0&&nr<8; nf+=df,nr+=dr) {
        int ns = ::sq(nf,nr);
        res |= bit(ns);
        if (occ & bit(ns)) break;
    }
    return res;
}
uint64_t Evaluator::rookAtk(int s, uint64_t occ) const {
    return rayAtk(s,occ,1,0)|rayAtk(s,occ,-1,0)|rayAtk(s,occ,0,1)|rayAtk(s,occ,0,-1);
}
uint64_t Evaluator::bishopAtk(int s, uint64_t occ) const {
    return rayAtk(s,occ,1,1)|rayAtk(s,occ,-1,1)|rayAtk(s,occ,1,-1)|rayAtk(s,occ,-1,-1);
}

bool Evaluator::isSquareAttacked(const Board& b, int s, Color by) const {
    const int f = ::file(s);
    const int r = rank(s);
    const uint64_t occ = b.allOccupied;

    // Pawn attacks
    if (by == WHITE) {
        if (f > 0 && r > 0 && (b.pieces[WHITE][PAWN] & bit(s - 9))) return true;
        if (f < 7 && r > 0 && (b.pieces[WHITE][PAWN] & bit(s - 7))) return true;
    } else {
        if (f > 0 && r < 7 && (b.pieces[BLACK][PAWN] & bit(s + 7))) return true;
        if (f < 7 && r < 7 && (b.pieces[BLACK][PAWN] & bit(s + 9))) return true;
    }

    // Knight attacks
    if (knightAtk[s] & b.pieces[by][KNIGHT]) return true;

    // Sliding attacks
    if (bishopAtk(s, occ) & (b.pieces[by][BISHOP] | b.pieces[by][QUEEN])) return true;
    if (rookAtk(s, occ) & (b.pieces[by][ROOK] | b.pieces[by][QUEEN])) return true;

    // King attacks
    uint64_t king = b.pieces[by][KING];
    if (king) {
        int ks = popLSB(king);
        int df = std::abs(::file(ks) - f);
        int dr = std::abs(rank(ks) - r);
        if (df <= 1 && dr <= 1) return true;
    }
    return false;
}

// ============================================================
//  Game phase (24 = full MG, 0 = pure EG)
// ============================================================
int Evaluator::calcPhase(const Board& b) const {
    int ph = 0;
    for (int c = 0; c < 2; c++) {
        ph += popcount(b.pieces[c][KNIGHT]) * 1;
        ph += popcount(b.pieces[c][BISHOP]) * 1;
        ph += popcount(b.pieces[c][ROOK])   * 2;
        ph += popcount(b.pieces[c][QUEEN])  * 4;
    }
    return std::min(ph, 24);
}

// ============================================================
//  PST — pure MG and EG (no blending inside)
// ============================================================
static int mirrorSq(int s) { return s ^ 56; }

int Evaluator::evalPST_MG(const Board& b, Color c) const {
    int score = 0;
    for (int p = 0; p < 6; p++) {
        uint64_t bb = b.pieces[c][p];
        while (bb) {
            int s = popLSB(bb);
            score += MG_PST[p][(c == WHITE) ? s : mirrorSq(s)];
        }
    }
    return score;
}

int Evaluator::evalPST_EG(const Board& b, Color c) const {
    int score = 0;
    for (int p = 0; p < 6; p++) {
        uint64_t bb = b.pieces[c][p];
        while (bb) {
            int s = popLSB(bb);
            int idx = (c == WHITE) ? s : mirrorSq(s);
            if      (p == KING) score += EG_KING_PST[idx];
            else if (p == PAWN) score += EG_PAWN_PST[idx];
            else                score += MG_PST[p][idx];
        }
    }
    return score;
}

// ============================================================
//  Pawn structure
// ============================================================
int Evaluator::evalPawns(const Board& b, Color c) const {
    int score = 0;
    uint64_t myPawns = b.pieces[c][PAWN];
    uint64_t bb = myPawns;
    while (bb) {
        int s = popLSB(bb);
        int f = ::file(s);
        if (!(myPawns & adjFileMask[f])) score -= 15; // isolated
        if (myPawns & fileMask[f] & ~bit(s)) score -= 10; // doubled
    }
    return score;
}

// ============================================================
//  Passed pawns — separate MG and EG (no blending)
// ============================================================
int Evaluator::evalPassedPawnMG(const Board& b, Color c) const {
    static const int BONUS[8] = {0, 5, 10, 20, 35, 55, 80, 0};
    int score = 0;
    uint64_t myPawns = b.pieces[c][PAWN];
    uint64_t thPawns = b.pieces[1-c][PAWN];
    uint64_t bb = myPawns;
    while (bb) {
        int s = popLSB(bb);
        if (!(passedMask[c][s] & thPawns))
            score += BONUS[(c == WHITE) ? rank(s) : (7 - rank(s))];
    }
    return score;
}

int Evaluator::evalPassedPawnEG(const Board& b, Color c) const {
    static const int BONUS[8] = {0, 15, 25, 45, 70, 105, 150, 0};
    int score = 0;
    uint64_t myPawns = b.pieces[c][PAWN];
    uint64_t thPawns = b.pieces[1-c][PAWN];
    uint64_t bb = myPawns;
    while (bb) {
        int s = popLSB(bb);
        if (!(passedMask[c][s] & thPawns))
            score += BONUS[(c == WHITE) ? rank(s) : (7 - rank(s))];
    }
    return score;
}

// ============================================================
//  Mobility
// ============================================================
int Evaluator::evalMobility(const Board& b, Color c) const {
    int score = 0;
    uint64_t occ = b.allOccupied;
    uint64_t own = b.occupied[c];

    uint64_t bb = b.pieces[c][KNIGHT];
    while (bb) score += popcount(knightAtk[popLSB(bb)] & ~own) * 4;

    bb = b.pieces[c][BISHOP];
    while (bb) score += popcount(bishopAtk(popLSB(bb), occ) & ~own) * 3;

    bb = b.pieces[c][ROOK];
    while (bb) score += popcount(rookAtk(popLSB(bb), occ) & ~own) * 2;

    bb = b.pieces[c][QUEEN];
    while (bb) {
        int s = popLSB(bb);
        score += popcount((rookAtk(s,occ)|bishopAtk(s,occ)) & ~own);
    }
    return score;
}

// ============================================================
//  Rooks
// ============================================================
int Evaluator::evalRooks(const Board& b, Color c) const {
    int score = 0;
    uint64_t myPawns = b.pieces[c][PAWN];
    uint64_t thPawns = b.pieces[1-c][PAWN];
    int rank7 = (c == WHITE) ? 6 : 1;

    uint64_t bb = b.pieces[c][ROOK];
    while (bb) {
        int s = popLSB(bb);
        int f = ::file(s);
        bool noMyPawn = !(myPawns & fileMask[f]);
        bool noThPawn = !(thPawns & fileMask[f]);
        if (noMyPawn && noThPawn) score += 20;
        else if (noMyPawn)        score += 10;
        if (rank(s) == rank7)     score += 20;
    }
    return score;
}

// ============================================================
//  King safety — pure MG term (outer blend handles tapering)
// ============================================================
int Evaluator::evalKingSafety(const Board& b, Color c) const {
    int ks  = b.kingSquare(c);
    int f   = ::file(ks), r = rank(ks);
    int fwd = (c == WHITE) ? 1 : -1;
    int score = 0;

    for (int df = -1; df <= 1; df++) {
        int nf = f + df;
        if (nf < 0 || nf > 7) continue;
        int sq1 = sq(nf, r + fwd);
        int sq2 = sq(nf, r + 2*fwd);
        if (r+fwd >= 0 && r+fwd < 8 && (b.pieces[c][PAWN] & bit(sq1)))   score += 12;
        if (r+2*fwd >= 0 && r+2*fwd < 8 && (b.pieces[c][PAWN] & bit(sq2))) score +=  5;
    }

    for (int df = -1; df <= 1; df++) {
        int nf = f + df;
        if (nf < 0 || nf > 7) continue;
        bool noFriend = !(b.pieces[c]  [PAWN] & fileMask[nf]);
        bool noEnemy  = !(b.pieces[1-c][PAWN] & fileMask[nf]);
        if (noFriend && noEnemy) score -= 22;
        else if (noFriend)       score -= 12;
    }
    return score;
}

// ============================================================
//  Bishop pair
// ============================================================
int Evaluator::evalBishopPair(const Board& b, Color c) const {
    return (popcount(b.pieces[c][BISHOP]) >= 2) ? 30 : 0;
}

int Evaluator::evalOutposts(const Board& b, Color c) const {
    int score = 0;
    uint64_t myPawns = b.pieces[c][PAWN];
    uint64_t enemyPawns = b.pieces[1-c][PAWN];
    uint64_t knights = b.pieces[c][KNIGHT];
    while (knights) {
        int s = popLSB(knights);
        int r = rank(s), f = ::file(s);
        if (c == WHITE && r < 3) continue;
        if (c == BLACK && r > 4) continue;

        bool supported = false;
        if (c == WHITE) {
            if (f > 0 && r > 0 && (myPawns & bit(::sq(f-1, r-1)))) supported = true;
            if (f < 7 && r > 0 && (myPawns & bit(::sq(f+1, r-1)))) supported = true;
        } else {
            if (f > 0 && r < 7 && (myPawns & bit(::sq(f-1, r+1)))) supported = true;
            if (f < 7 && r < 7 && (myPawns & bit(::sq(f+1, r+1)))) supported = true;
        }
        if (!supported) continue;

        bool attackableByPawn = false;
        if (c == WHITE) {
            for (int rr = r + 1; rr < 8; rr++) {
                if (f > 0 && (enemyPawns & bit(::sq(f-1, rr)))) attackableByPawn = true;
                if (f < 7 && (enemyPawns & bit(::sq(f+1, rr)))) attackableByPawn = true;
            }
        } else {
            for (int rr = r - 1; rr >= 0; rr--) {
                if (f > 0 && (enemyPawns & bit(::sq(f-1, rr)))) attackableByPawn = true;
                if (f < 7 && (enemyPawns & bit(::sq(f+1, rr)))) attackableByPawn = true;
            }
        }
        if (!attackableByPawn) score += 22;
    }
    return score;
}

int Evaluator::evalThreats(const Board& b, Color c) const {
    int score = 0;
    Color them = (Color)(1-c);
    static constexpr int THREAT_W[6]  = { 6, 14, 14, 22, 36, 0};
    static constexpr int HANGING_W[6] = {10, 24, 24, 34, 54, 0};

    // Reward pressure on opponent pieces, especially hanging ones.
    uint64_t enemyPieces = b.occupied[them] & ~b.pieces[them][KING];
    while (enemyPieces) {
        int s = popLSB(enemyPieces);
        int pt = b.pieceOn[s];
        if (pt < PAWN || pt > QUEEN) continue;
        bool attacked = isSquareAttacked(b, s, c);
        if (!attacked) continue;
        bool defended = isSquareAttacked(b, s, them);
        score += defended ? THREAT_W[pt] : HANGING_W[pt];
    }

    // Penalize own hanging pieces so eval bar reflects tactical danger.
    uint64_t ownPieces = b.occupied[c] & ~b.pieces[c][KING];
    while (ownPieces) {
        int s = popLSB(ownPieces);
        int pt = b.pieceOn[s];
        if (pt < PAWN || pt > QUEEN) continue;
        bool attacked = isSquareAttacked(b, s, them);
        if (!attacked) continue;
        bool defended = isSquareAttacked(b, s, c);
        score -= defended ? (THREAT_W[pt] / 2) : (HANGING_W[pt] * 2 / 3);
    }
    return score;
}

int Evaluator::evalSpace(const Board& b, Color c) const {
    int score = 0;
    uint64_t own = b.occupied[c];
    uint64_t occ = b.allOccupied;

    uint64_t bb = b.pieces[c][KNIGHT];
    while (bb) {
        int s = popLSB(bb);
        uint64_t atk = knightAtk[s] & ~own;
        uint64_t center = atk & 0x00003C3C3C3C0000ULL;
        score += popcount(center) * 2;
    }
    bb = b.pieces[c][BISHOP];
    while (bb) {
        int s = popLSB(bb);
        uint64_t center = bishopAtk(s, occ) & ~own & 0x00003C3C3C3C0000ULL;
        score += popcount(center);
    }
    return score;
}

// ============================================================
//  Top-level evaluate — single tapered blend at the end
// ============================================================
int Evaluator::evaluate(const Board& b) const {
    int phase = calcPhase(b);
    int mg = 0, eg = 0;

    for (int c = 0; c < 2; c++) {
        int sign = (c == (int)b.sideToMove) ? 1 : -1;

        // Material (different MG/EG values)
        for (int p = 0; p < 5; p++) {
            int n = popcount(b.pieces[c][p]);
            mg += sign * n * MG_VAL[p];
            eg += sign * n * EG_VAL[p];
        }

        // PST — pure MG table -> mg, pure EG table -> eg
        mg += sign * evalPST_MG(b, (Color)c);
        eg += sign * evalPST_EG(b, (Color)c);

        // Pawn structure (same penalty in both phases)
        int pw = evalPawns(b, (Color)c);
        mg += sign * pw;
        eg += sign * pw;

        // Passed pawns — separate MG/EG bonuses
        mg += sign * evalPassedPawnMG(b, (Color)c);
        eg += sign * evalPassedPawnEG(b, (Color)c);

        // Mobility — stronger in MG, half in EG
        int mob = evalMobility(b, (Color)c);
        mg += sign * mob;
        eg += sign * (mob / 2);

        // Rooks (useful in both phases)
        int rk = evalRooks(b, (Color)c);
        mg += sign * rk;
        eg += sign * rk;

        // King safety — MG only (tapers naturally to 0 as phase -> 0)
        mg += sign * evalKingSafety(b, (Color)c);

        // Bishop pair
        int bp = evalBishopPair(b, (Color)c);
        mg += sign * bp;
        eg += sign * bp;

        int out = evalOutposts(b, (Color)c);
        mg += sign * out;
        eg += sign * (out / 2);

        int thr = evalThreats(b, (Color)c);
        mg += sign * thr;
        eg += sign * (thr / 2);

        int sp = evalSpace(b, (Color)c);
        mg += sign * sp;
    }

    return (mg * phase + eg * (24 - phase)) / 24;
}
