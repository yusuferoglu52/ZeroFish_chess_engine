#pragma once
#include "Board.h"
#include "MoveGenerator.h"
#include "Evaluator.h"
#include <vector>
#include <optional>
#include <cmath>
#include <chrono>

struct TTEntry {
    uint64_t hash;
    int      depth;
    int      score;
    uint8_t  flag;     // 0=exact, 1=lower(beta cut), 2=upper(alpha fail)
    Move     bestMove;
};

class Engine {
public:
    struct SearchTuning {
        int nullMargin = 35;
        int futilityD1 = 200;
        int futilityD2 = 380;
        double lmrDivisor = 2.25;
    };

    Engine();
    void setSearchTuning(const SearchTuning& t);
    const SearchTuning& getSearchTuning() const { return tuning; }

    Move findBestMove(Board& board, int maxDepth, int maxTimeMs = 0, long long maxNodes = 0);

    long long nodesSearched  = 0;
    int       maxDepthReached = 0;
    int       lastScore       = 0; // score from last search (sideToMove perspective)
    bool      verbose         = true;

    static constexpr int INF        = 1'000'000;
    static constexpr int MATE_SCORE = 900'000;

private:
    MoveGenerator moveGen;
    Evaluator     eval;

    // Transposition table — 2M entries
    static constexpr size_t TT_SIZE = 1 << 21;
    std::vector<TTEntry> tt;

    TTEntry* ttProbe(uint64_t hash);
    void     ttStore(uint64_t hash, int depth, int score, uint8_t flag, const Move& best);

    // Negamax + PVS + LMR + null move + check extension
    int negamax(Board& board, int depth, int alpha, int beta, int ply, bool canNull = true);
    int quiescence(Board& board, int alpha, int beta);
    int seeCapture(Board& board, const Move& m);
    bool attacksSquare(const Board& board, int from, PieceType pt, Color side, int target) const;
    bool isLegalRecapture(Board& board, const Move& m, Color side);
    bool hasNonPawnMaterial(const Board& board, Color side) const;

    // Move ordering
    void orderMoves(std::vector<Move>& moves, Board& board,
                    const Move* ttMove, int ply);

    // Killer moves [ply][slot]
    Move killers[Board::MAX_DEPTH][2];

    // History heuristic [color][from][to]
    int history[2][64][64];

    // Late Move Reduction table [depth][moveIndex]
    int lmrTable[64][64];

    void clearKillers();
    void clearHistory();
    void storeKiller(const Move& m, int ply);
    void initLMR();
    bool shouldStop() const;

    int maxTimeMsLimit = 0;
    long long maxNodesLimit = 0;
    std::chrono::steady_clock::time_point searchStart;
    SearchTuning tuning;
};
