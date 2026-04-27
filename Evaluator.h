#pragma once
#include "Board.h"

class Evaluator {
public:
    Evaluator();
    int evaluate(const Board& board) const;

    static constexpr int MG_VAL[6] = {100, 320, 330, 500, 900, 20000};
    static constexpr int EG_VAL[6] = {120, 290, 310, 520, 920, 20000};

    static const int MG_PST[6][64];
    static const int EG_KING_PST[64];
    static const int EG_PAWN_PST[64];

private:
    uint64_t knightAtk[64];
    uint64_t passedMask[2][64];
    uint64_t fileMask[8];
    uint64_t adjFileMask[8];

    static uint64_t rayAtk(int sq, uint64_t occ, int df, int dr);
    uint64_t rookAtk  (int sq, uint64_t occ) const;
    uint64_t bishopAtk(int sq, uint64_t occ) const;
    bool isSquareAttacked(const Board& b, int sq, Color by) const;

    int calcPhase(const Board& b) const;

    // Pure MG and EG PST scores (no blending inside)
    int evalPST_MG(const Board& b, Color c) const;
    int evalPST_EG(const Board& b, Color c) const;

    int evalPawns        (const Board& b, Color c) const;
    int evalPassedPawnMG (const Board& b, Color c) const;
    int evalPassedPawnEG (const Board& b, Color c) const;
    int evalMobility     (const Board& b, Color c) const;
    int evalRooks        (const Board& b, Color c) const;
    int evalKingSafety   (const Board& b, Color c) const;
    int evalBishopPair   (const Board& b, Color c) const;
    int evalOutposts     (const Board& b, Color c) const;
    int evalThreats      (const Board& b, Color c) const;
    int evalSpace        (const Board& b, Color c) const;
};
