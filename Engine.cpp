#include "Engine.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cmath>

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

bool Engine::attacksSquare(const Board& board, int from, PieceType pt, Color side, int target) const {
    int ff = ::file(from), fr = rank(from);
    int tf = ::file(target), tr = rank(target);
    int df = tf - ff, dr = tr - fr;
    int adf = std::abs(df), adr = std::abs(dr);

    auto clearRay = [&](int stepF, int stepR) -> bool {
        int nf = ff + stepF, nr = fr + stepR;
        while (nf != tf || nr != tr) {
            if (board.allOccupied & bit(::sq(nf, nr))) return false;
            nf += stepF; nr += stepR;
        }
        return true;
    };

    switch (pt) {
        case PAWN:
            return (side == WHITE) ? (dr == 1 && adf == 1) : (dr == -1 && adf == 1);
        case KNIGHT:
            return (adf == 1 && adr == 2) || (adf == 2 && adr == 1);
        case BISHOP:
            return adf == adr && clearRay((df > 0) ? 1 : -1, (dr > 0) ? 1 : -1);
        case ROOK:
            if (df == 0 && dr != 0) return clearRay(0, (dr > 0) ? 1 : -1);
            if (dr == 0 && df != 0) return clearRay((df > 0) ? 1 : -1, 0);
            return false;
        case QUEEN:
            if (adf == adr) return clearRay((df > 0) ? 1 : -1, (dr > 0) ? 1 : -1);
            if (df == 0 && dr != 0) return clearRay(0, (dr > 0) ? 1 : -1);
            if (dr == 0 && df != 0) return clearRay((df > 0) ? 1 : -1, 0);
            return false;
        case KING:
            return adf <= 1 && adr <= 1;
        default:
            return false;
    }
}

bool Engine::isLegalRecapture(Board& board, const Move& m, Color side) {
    board.makeMove(m);
    bool legal = !moveGen.isInCheck(board, side);
    board.unmakeMove(m);
    return legal;
}

int Engine::seeCapture(Board& board, const Move& m) {
    if (m.enPassant || m.captured == NO_PIECE) return 0;

    Board tmp = board;
    int gain[32]{};
    int depth = 0;
    gain[0] = SEE_VALUE[m.captured];
    int target = m.to;

    tmp.makeMove(m);
    Color side = tmp.sideToMove;

    while (true) {
        int bestFrom = -1;
        PieceType bestPt = NO_PIECE;
        for (int pt = PAWN; pt <= KING && bestFrom == -1; pt++) {
            uint64_t bb = tmp.pieces[side][pt];
            while (bb) {
                int from = popLSB(bb);
                if (!attacksSquare(tmp, from, (PieceType)pt, side, target)) continue;
                Move rec((uint8_t)from, (uint8_t)target, (uint8_t)pt, tmp.pieceOn[target]);
                if (!isLegalRecapture(tmp, rec, side)) continue;
                bestFrom = from;
                bestPt = (PieceType)pt;
                break;
            }
        }

        if (bestFrom == -1) break;
        if (depth + 1 >= 31) break;

        depth++;
        gain[depth] = SEE_VALUE[tmp.pieceOn[target]] - gain[depth - 1];
        Move rec((uint8_t)bestFrom, (uint8_t)target, (uint8_t)bestPt, tmp.pieceOn[target]);
        tmp.makeMove(rec);
        side = tmp.sideToMove;
    }

    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        depth--;
    }

    return gain[0];
}

void Engine::orderMoves(std::vector<Move>& moves, Board& board,
                        const Move* ttMove, int ply)
{
    Color us = board.sideToMove;
    auto score = [&](const Move& m) -> int {
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
    std::sort(moves.begin(), moves.end(),
              [&](const Move& a, const Move& b){ return score(a) > score(b); });
}

// ---- Quiescence search ----
int Engine::quiescence(Board& board, int alpha, int beta) {
    if (shouldStop()) return 0;
    nodesSearched++;
    if (board.isDraw()) return 0;

    int stand = eval.evaluate(board);
    if (stand >= beta) return beta;
    // Delta pruning: skip if even capturing a queen won't help
    if (stand + 1000 < alpha) return alpha;
    if (stand > alpha) alpha = stand;

    auto caps = moveGen.generateCaptures(board);
    orderMoves(caps, board, nullptr, 0);

    for (auto& m : caps) {
        if (shouldStop()) break;
        if (!m.enPassant && m.captured != NO_PIECE) {
            // Avoid dropping tactical defensive captures too aggressively.
            int see = seeCapture(board, m);
            if (see < -120) continue;
        }
        board.makeMove(m);
        int score = -quiescence(board, -beta, -alpha);
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

    bool inCheck = moveGen.isInCheck(board, board.sideToMove);

    // Check extension — don't stop at horizon while in check
    if (inCheck) depth++;

    // TT probe
    TTEntry* tte = ttProbe(board.hash);
    Move* ttMove = nullptr;
    if (tte) {
        ttMove = &tte->bestMove;
        if (tte->depth >= depth) {
            if      (tte->flag == 0) return tte->score;
            else if (tte->flag == 1) alpha = std::max(alpha, tte->score);
            else                     beta  = std::min(beta,  tte->score);
            if (alpha >= beta) return tte->score;
        }
    }

    if (depth <= 0) return quiescence(board, alpha, beta);

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

        int score;
        if (i == 0) {
            // First move: full window (PVS — expect this to be best)
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, true);
        } else {
            // LMR: reduce depth for late quiet moves
            int reduction = 0;
            if (isQuiet && !inCheck && depth >= 3 && i >= 3)
                reduction = lmrTable[std::min(63, depth)][std::min(63, i)];

            // Null-window search at reduced depth
            score = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);

            if (isQuiet && reduction > 0 && moveGen.isInCheck(board, board.sideToMove))
                reduction = 0;

            // If LMR failed high, re-search without reduction
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
            ttStore(board.hash, depth, beta, 1, m);
            return beta;
        }
    }

    uint8_t flag = (bestScore <= origAlpha) ? 2 : 0;
    if (bestScore == -INF) return alpha;
    ttStore(board.hash, depth, bestScore, flag, bestMove);
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
        int alpha = (depth >= 5) ? prevScore - 50 : -INF;
        int beta  = (depth >= 5) ? prevScore + 50 :  INF;

        Move iterBest  = rootMoves[0];
        int  iterScore = -INF;
        bool research  = false;
        bool depthComplete = true;

        do {
            research  = false;
            iterScore = -INF;
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
                if (iterScore <= prevScore - 50)      { alpha = -INF; research = true; }
                else if (iterScore >= prevScore + 50) { beta  =  INF; research = true; }
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
