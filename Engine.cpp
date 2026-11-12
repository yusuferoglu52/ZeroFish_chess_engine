#include "Engine.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cmath>
#include <utility>

namespace {
// Mate scores must be stored relative to the current node, not the root, or a TT
// hit at a different ply reports the wrong mate distance. Adjust on store/probe.
constexpr int MATE_THRESHOLD = Engine::MATE_SCORE - 1000;
inline int scoreToTT(int score, int ply) {
    if (score >=  MATE_THRESHOLD) return score + ply;
    if (score <= -MATE_THRESHOLD) return score - ply;
    return score;
}
inline int scoreFromTT(int score, int ply) {
    if (score >=  MATE_THRESHOLD) return score - ply;
    if (score <= -MATE_THRESHOLD) return score + ply;
    return score;
}
} // namespace

Engine::Engine() : tt(TT_SIZE) {
    clearKillers();
    clearHistory();
    initLMR();
}

void Engine::setSearchTuning(const SearchTuning& t) {
    tuning = t;
    initLMR();
}

bool Engine::shouldStop() const {
    if (maxNodesLimit > 0 && nodesSearched >= maxNodesLimit) return true;
    if (maxTimeMsLimit > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - searchStart
        ).count();
        if (elapsed >= maxTimeMsLimit) return true;
    }
    return false;
}

void Engine::initLMR() {
    for (int d = 0; d < 64; d++)
        for (int m = 0; m < 64; m++) {
            if (d < 3 || m < 3) lmrTable[d][m] = 0;
            else lmrTable[d][m] = std::max(1, (int)(0.75 + std::log(d) * std::log(m) / tuning.lmrDivisor));
        }
}

void Engine::clearKillers() { memset(killers, 0, sizeof(killers)); }
void Engine::clearHistory()  { memset(history, 0, sizeof(history)); }

void Engine::storeKiller(const Move& m, int ply) {
    if (killers[ply][0].from==m.from && killers[ply][0].to==m.to) return;
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = m;
}

// ---- Transposition table ----
TTEntry* Engine::ttProbe(uint64_t hash) {
    TTEntry& e = tt[hash % TT_SIZE];
    return (e.hash == hash) ? &e : nullptr;
}
void Engine::ttStore(uint64_t hash, int depth, int score, uint8_t flag, const Move& best) {
    TTEntry& e = tt[hash % TT_SIZE];
    // Prefer deeper entries; always replace on same depth
    if (e.hash != hash || e.depth <= depth)
        e = {hash, depth, score, flag, best};
}

// ---- Move ordering ----
static const int MVV_LVA[6][6] = {
//         P    N    B    R    Q    K  (attacker)
/* P */ { 15,  14,  13,  12,  11,  10 },
/* N */ { 25,  24,  23,  22,  21,  20 },
/* B */ { 35,  34,  33,  32,  31,  30 },
/* R */ { 45,  44,  43,  42,  41,  40 },
/* Q */ { 55,  54,  53,  52,  51,  50 },
/* K */ {  0,   0,   0,   0,   0,   0 },
};
static const int SEE_VALUE[6] = {100, 320, 330, 500, 900, 20000};

bool Engine::hasNonPawnMaterial(const Board& board, Color side) const {
    return (board.pieces[side][KNIGHT] | board.pieces[side][BISHOP] |
            board.pieces[side][ROOK]   | board.pieces[side][QUEEN]) != 0ULL;
}

// Static exchange evaluation via the classical swap algorithm. Operates purely on a
// local occupancy mask — no Board copy, no make/unmake — so it is cheap enough to call
// during move ordering. (Pins are ignored, which is standard for SEE.)
int Engine::seeCapture(Board& board, const Move& m) {
    if (m.enPassant || m.captured == NO_PIECE) return 0;

    const int to = m.to;
    int gain[32];
    int d = 0;

    uint64_t occ = board.allOccupied ^ bit(m.from); // moving piece leaves its origin
    PieceType onSquare = (PieceType)m.piece;         // piece value now standing on 'to'
    gain[0] = SEE_VALUE[m.captured];
    Color side = (Color)(1 - board.sideToMove);      // side to recapture next

    while (true) {
        uint64_t attackers = moveGen.attackersTo(board, to, occ) & occ;
        uint64_t sideAtt   = attackers & board.occupied[side];
        if (!sideAtt) break;

        // Least valuable attacker for the side to move.
        PieceType lva = KING;
        uint64_t  lvaBit = 0;
        for (int pt = PAWN; pt <= KING; pt++) {
            uint64_t b = sideAtt & board.pieces[side][pt];
            if (b) { lva = (PieceType)pt; lvaBit = b & (0ULL - b); break; }
        }

        // A king may only capture if the defender has no attacker left on the square.
        if (lva == KING && (attackers & board.occupied[1 - side])) break;

        if (d + 1 >= 32) break;
        d++;
        gain[d] = SEE_VALUE[onSquare] - gain[d - 1];
        onSquare = lva;
        occ ^= lvaBit;                                // remove that attacker
        side = (Color)(1 - side);
    }

    while (d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
        d--;
    }
    return gain[0];
}

void Engine::orderMoves(std::vector<Move>& moves, Board& board,
                        const Move* ttMove, int ply)
{
    Color us = board.sideToMove;
    auto scoreMove = [&](const Move& m) -> int {
        if (ttMove && m.from==ttMove->from && m.to==ttMove->to) return 1'000'000;
        if (m.captured != NO_PIECE || m.enPassant) {
            int victim = (m.captured == NO_PIECE) ? SEE_VALUE[PAWN] : SEE_VALUE[m.captured];
            int see = seeCapture(board, m);
            int capIdx = (m.captured == NO_PIECE) ? (int)PAWN : (int)m.captured;
            if (see >= 0) return 120'000 + see + MVV_LVA[capIdx][m.piece];
            return 40'000 + see + (victim / 4);
        }
        if (m.promoted != NO_PIECE) return  90'000 + m.promoted;
        if (m.from==killers[ply][0].from && m.to==killers[ply][0].to) return 80'000;
        if (m.from==killers[ply][1].from && m.to==killers[ply][1].to) return 70'000;
        return history[us][m.from][m.to];
    };

    // Score every move exactly once (SEE is too costly to recompute inside a sort
    // comparator), then sort indices by the precomputed scores.
    const int n = (int)moves.size();
    std::vector<std::pair<int,int>> order;
    order.reserve(n);
    for (int i = 0; i < n; i++) order.emplace_back(scoreMove(moves[i]), i);
    std::stable_sort(order.begin(), order.end(),
                     [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                         return a.first > b.first;
                     });
    std::vector<Move> sorted;
    sorted.reserve(n);
    for (const auto& pr : order) sorted.push_back(moves[pr.second]);
    moves.swap(sorted);
}

// ---- Quiescence search ----
int Engine::quiescence(Board& board, int alpha, int beta, int ply) {
    if (shouldStop()) return 0;
    nodesSearched++;
    if (board.isDraw()) return 0;
    if (ply >= Board::MAX_DEPTH - 1) return eval.evaluate(board);

    bool inCheck = moveGen.isInCheck(board, board.sideToMove);

    if (inCheck) {
        // In check there is no legal "pass", so we cannot stand pat — search all
        // evasions instead, otherwise the horizon eval is illegally optimistic.
        auto moves = moveGen.generateMoves(board);
        if (moves.empty()) return -(MATE_SCORE - ply); // checkmate
        orderMoves(moves, board, nullptr, ply);
        for (auto& m : moves) {
            if (shouldStop()) break;
            board.makeMove(m);
            int score = -quiescence(board, -beta, -alpha, ply + 1);
            board.unmakeMove(m);
            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
        return alpha;
    }

    int stand = eval.evaluate(board);
    if (stand >= beta) return beta;
    // Delta pruning: skip if even capturing a queen won't help
    if (stand + 1000 < alpha) return alpha;
    if (stand > alpha) alpha = stand;

    auto caps = moveGen.generateCaptures(board);
    orderMoves(caps, board, nullptr, ply);

    for (auto& m : caps) {
        if (shouldStop()) break;
        if (!m.enPassant && m.captured != NO_PIECE) {
            // Avoid dropping tactical defensive captures too aggressively.
            int see = seeCapture(board, m);
            if (see < -120) continue;
        }
        board.makeMove(m);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(m);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// ---- Negamax + Alpha-Beta + PVS + LMR + Null Move ----
int Engine::negamax(Board& board, int depth, int alpha, int beta, int ply, bool canNull) {
    if (shouldStop()) return 0;
    nodesSearched++;
    if (board.isDraw()) return 0;

    // Hard ply ceiling: bound recursion so killers/undo stacks can never overflow.
    if (ply >= Board::MAX_DEPTH - 1) return eval.evaluate(board);

    bool inCheck = moveGen.isInCheck(board, board.sideToMove);

    // Check extension — don't stop at horizon while in check
    if (inCheck) depth++;

    // TT probe
    TTEntry* tte = ttProbe(board.hash);
    Move* ttMove = nullptr;
    if (tte) {
        ttMove = &tte->bestMove;
        if (tte->depth >= depth) {
            int ttScore = scoreFromTT(tte->score, ply);
            if      (tte->flag == 0) return ttScore;
            else if (tte->flag == 1) alpha = std::max(alpha, ttScore);
            else                     beta  = std::min(beta,  ttScore);
            if (alpha >= beta) return ttScore;
        }
    }

    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    Color us = board.sideToMove;

    // Null move pruning — avoid in tactical/low-material spots
    int staticEval = eval.evaluate(board);
    if (canNull && !inCheck && depth >= 3 && ply > 0 && hasNonPawnMaterial(board, us) &&
        staticEval >= beta - tuning.nullMargin) {
            int R = (depth >= 6) ? 3 : 2;
            board.makeNullMove();
            int nullScore = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
            board.unmakeNullMove();
            if (nullScore >= beta) return beta;
    }

    // Futility pruning — at shallow depth, skip quiet moves that can't raise alpha
    bool futility = false;
    if (!inCheck && depth <= 2) {
        int staticEvalLocal = staticEval;
        int margin = (depth == 1) ? tuning.futilityD1 : tuning.futilityD2;
        if (staticEvalLocal + margin <= alpha) futility = true;
    }

    auto moves = moveGen.generateMoves(board);
    if (moves.empty()) return inCheck ? -(MATE_SCORE - ply) : 0;

    orderMoves(moves, board, ttMove, ply);

    int  origAlpha = alpha;
    Move bestMove  = moves[0];
    int  bestScore = -INF;

    for (int i = 0; i < (int)moves.size(); i++) {
        if (shouldStop()) break;
        const Move& m = moves[i];
        bool isCapture = (m.captured != NO_PIECE || m.enPassant);
        bool isPromo   = (m.promoted != NO_PIECE);
        bool isQuiet   = !isCapture && !isPromo;

        // Futility pruning: skip quiet moves beyond the first
        if (futility && isQuiet && i > 0) continue;

        board.makeMove(m);
        bool givesCheck = moveGen.isInCheck(board, board.sideToMove);

        int score;
        if (i == 0) {
            // First move: full window (PVS — expect this to be best)
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, true);
        } else {
            // LMR: reduce depth for late quiet moves — but never reduce checking
            // moves (decided up front, so the reduced score is trustworthy).
            int reduction = 0;
            if (isQuiet && !inCheck && !givesCheck && depth >= 3 && i >= 3)
                reduction = lmrTable[std::min(63, depth)][std::min(63, i)];

            // Null-window search at reduced depth
            score = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);

            // If LMR failed high, re-search at full depth
            if (reduction > 0 && score > alpha)
                score = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, true);

            // If null-window failed high, do full re-search
            if (score > alpha && score < beta)
                score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, true);
        }

        board.unmakeMove(m);

        if (score > bestScore) { bestScore = score; bestMove = m; }
        if (score > alpha)     alpha = score;
        if (alpha >= beta) {
            if (isQuiet) {
                storeKiller(m, ply);
                history[(int)us][m.from][m.to] += depth * depth;
            }
            ttStore(board.hash, depth, scoreToTT(beta, ply), 1, m);
            return beta;
        }
    }

    uint8_t flag = (bestScore <= origAlpha) ? 2 : 0;
    if (bestScore == -INF) return alpha;
    ttStore(board.hash, depth, scoreToTT(bestScore, ply), flag, bestMove);
    return bestScore;
}

// ---- Iterative Deepening root search ----
Move Engine::findBestMove(Board& board, int maxDepth, int maxTimeMs, long long maxNodes) {
    nodesSearched = 0;
    clearKillers();
    clearHistory();
    maxTimeMsLimit = std::max(0, maxTimeMs);
    maxNodesLimit = std::max(0LL, maxNodes);
    searchStart = std::chrono::steady_clock::now();
    const bool hasSearchLimits = (maxTimeMsLimit > 0) || (maxNodesLimit > 0);
    const bool autoDepthMode = (maxDepth <= 0);

    auto rootMoves = moveGen.generateMoves(board);
    if (rootMoves.empty()) return Move{};

    Move best = rootMoves[0];
    int prevScore = 0;

    auto searchOneDepth = [&](int depth) -> bool {
        if (shouldStop()) return false;
        // Aspiration window (only from depth 5 up)
        int windowAlpha = (depth >= 5) ? prevScore - 50 : -INF;
        int windowBeta  = (depth >= 5) ? prevScore + 50 :  INF;

        Move iterBest  = rootMoves[0];
        int  iterScore = -INF;
        bool research  = false;
        bool depthComplete = true;

        do {
            research  = false;
            iterScore = -INF;
            // Fresh search bounds each pass — the alpha that climbs during the move
            // loop must not leak into a widened re-search.
            int alpha = windowAlpha;
            int beta  = windowBeta;
            orderMoves(rootMoves, board, &best, 0);

            for (int i = 0; i < (int)rootMoves.size(); i++) {
                if (shouldStop()) { depthComplete = false; break; }
                auto& m = rootMoves[i];
                board.makeMove(m);
                int score;
                if (i == 0) {
                    score = -negamax(board, depth - 1, -beta, -alpha, 1, true);
                } else {
                    score = -negamax(board, depth - 1, -alpha - 1, -alpha, 1, true);
                    if (score > alpha && score < beta)
                        score = -negamax(board, depth - 1, -beta, -alpha, 1, true);
                }
                board.unmakeMove(m);

                if (score > iterScore) { iterScore = score; iterBest = m; }
                if (score > alpha) alpha = score;
            }
            if (!depthComplete) break;

            // Widen window if score fell outside (rare)
            if (depth >= 5) {
                if (iterScore <= windowAlpha)     { windowAlpha = -INF; research = true; }
                else if (iterScore >= windowBeta) { windowBeta  =  INF; research = true; }
            }
        } while (research && !shouldStop());

        if (!depthComplete || iterScore == -INF) return false;

        best      = iterBest;
        prevScore = iterScore;
        lastScore = iterScore;
        maxDepthReached = depth;

        auto sqName = [](int s) -> std::string {
            std::string n; n += (char)('a' + ::file(s)); n += (char)('1' + rank(s)); return n;
        };
        if (verbose) {
            std::cout << "depth " << depth
                      << " score " << iterScore
                      << " nodes " << nodesSearched
                      << " move "  << sqName(best.from) << sqName(best.to);
            if (best.promoted != NO_PIECE) std::cout << "pnbrqk"[best.promoted];
            std::cout << "\n";
        }

        if (shouldStop()) return false;
        if (!hasSearchLimits && std::abs(iterScore) >= MATE_SCORE - 200) return false;
        return true;
    };

    if (autoDepthMode) {
        const int depthCeiling = hasSearchLimits ? (Board::MAX_DEPTH - 1) : std::max(1, Board::MAX_DEPTH - 1);
        for (int depth = 1; depth <= depthCeiling; depth++) {
            if (!searchOneDepth(depth)) break;
        }
    } else {
        const int fixedDepth = std::max(1, maxDepth);
        if (hasSearchLimits) {
            while (!shouldStop()) {
                if (!searchOneDepth(fixedDepth)) break;
            }
        } else {
            (void)searchOneDepth(fixedDepth);
        }
    }
    return best;
}
