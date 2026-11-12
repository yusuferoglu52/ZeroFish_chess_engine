#pragma once
#include "Board.h"
#include <vector>

class MoveGenerator {
public:
    MoveGenerator();

    // Generate all pseudo-legal moves; filter to legal via isInCheck
    std::vector<Move> generateMoves(Board& board);

    // Generate only captures (for quiescence)
    std::vector<Move> generateCaptures(Board& board);

    // Returns true if 'color' king is in check
    bool isInCheck(const Board& board, Color color) const;

    // Is square attacked by any piece of 'by' color?
    bool isSquareAttacked(const Board& board, int sq, Color by) const;

    // Bitboard of every piece (both colors) attacking 'sq' for a given occupancy.
    // Used by the engine's static exchange evaluation.
    uint64_t attackersTo(const Board& board, int sq, uint64_t occ) const;

private:
    // Pre-computed attack tables
    uint64_t knightAttacks[64];
    uint64_t kingAttacks[64];
    uint64_t pawnAttacks[2][64]; // [color][square]

    void initAttackTables();

    // Sliding piece attacks (classical approach)
    uint64_t rookAttacks(int sq, uint64_t occ)   const;
    uint64_t bishopAttacks(int sq, uint64_t occ) const;
    uint64_t queenAttacks(int sq, uint64_t occ)  const;

    // Per-piece move generators (pseudo-legal)
    void genPawnMoves   (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genKnightMoves (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genBishopMoves (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genRookMoves   (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genQueenMoves  (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genKingMoves   (const Board& b, Color us, std::vector<Move>& ml, bool capturesOnly) const;
    void genCastling    (const Board& b, Color us, std::vector<Move>& ml) const;

    void addMove(std::vector<Move>& ml, const Move& m) const { ml.push_back(m); }
};
