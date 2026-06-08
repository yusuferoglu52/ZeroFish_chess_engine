// Minimal UCI front-end for the engine — lets GUIs (Arena, CuteChess) and
// match runners (for SPRT / strength testing) drive ZeroFish.
//
// Supported: uci, isready, ucinewgame, position (startpos|fen ... [moves ...]),
//            go (depth|movetime|nodes|wtime/btime/winc/binc/movestogo|infinite),
//            stop, quit.
//
// Note: search runs synchronously, so "go infinite" / "stop" are approximated by
// a time cap rather than true ponder/stop (which would need an async search).
#include "Board.h"
#include "MoveGenerator.h"
#include "Engine.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static std::string moveToUci(const Move& m) {
    if (m.from == m.to) return "0000"; // null / no legal move
    std::string s;
    s += (char)('a' + ::file(m.from));
    s += (char)('1' + rank(m.from));
    s += (char)('a' + ::file(m.to));
    s += (char)('1' + rank(m.to));
    if (m.promoted != NO_PIECE) s += "pnbrqk"[m.promoted];
    return s;
}

// Apply a UCI move string by matching it against the legal move list (so castling,
// en passant and promotions are resolved without re-deriving move flags here).
static bool applyUciMove(Board& b, MoveGenerator& gen, const std::string& uci) {
    auto legal = gen.generateMoves(b);
    for (const auto& m : legal)
        if (moveToUci(m) == uci) { b.makeMove(m); return true; }
    return false;
}

static void parsePosition(std::istringstream& ss, Board& board, MoveGenerator& gen) {
    std::string token;
    ss >> token;
    if (token == "startpos") {
        board.setStartPosition();
        ss >> token; // consume "moves" if present
    } else if (token == "fen") {
        std::string fen, part;
        for (int i = 0; i < 6 && (ss >> part); i++) {
            if (i) fen += ' ';
            fen += part;
        }
        board.parseFEN(fen);
        ss >> token; // consume "moves" if present
    }
    if (token == "moves") {
        std::string mv;
        while (ss >> mv) applyUciMove(board, gen, mv);
    }
}

static void handleGo(std::istringstream& ss, Board& board, Engine& engine) {
    int depth = 0, movetime = 0;
    long long nodes = 0;
    int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 0;
    bool infinite = false;

    std::string token;
    while (ss >> token) {
        if      (token == "depth")     ss >> depth;
        else if (token == "movetime")  ss >> movetime;
        else if (token == "nodes")     ss >> nodes;
        else if (token == "wtime")     ss >> wtime;
        else if (token == "btime")     ss >> btime;
        else if (token == "winc")      ss >> winc;
        else if (token == "binc")      ss >> binc;
        else if (token == "movestogo") ss >> movestogo;
        else if (token == "infinite")  infinite = true;
    }

    int timeBudget = 0;
    if (movetime > 0) {
        timeBudget = movetime;
    } else if (!infinite && (wtime > 0 || btime > 0)) {
        int myTime = (board.sideToMove == WHITE) ? wtime : btime;
        int myInc  = (board.sideToMove == WHITE) ? winc  : binc;
        int moves  = (movestogo > 0) ? movestogo : 30;
        timeBudget = myTime / std::max(1, moves) + myInc / 2;
        int cap = (myTime > 100) ? myTime - 50 : myTime / 2; // keep a safety margin
        timeBudget = std::max(1, std::min(timeBudget, cap));
    }

    int searchDepth = (depth > 0) ? depth : 0; // 0 => iterative auto-deepen
    if (infinite) {
        searchDepth = 0;
        timeBudget  = 5000; // synchronous cap in lieu of real ponder/stop
    } else if (searchDepth == 0 && timeBudget == 0 && nodes == 0) {
        timeBudget = 1000; // sane default when nothing is specified
    }

    Board copy = board;
    Move best = engine.findBestMove(copy, searchDepth, timeBudget, nodes);

    int score = engine.lastScore;
    std::cout << "info depth " << engine.maxDepthReached;
    if (std::abs(score) >= Engine::MATE_SCORE - 1000) {
        int mateIn = (Engine::MATE_SCORE - std::abs(score) + 1) / 2;
        std::cout << " score mate " << (score > 0 ? mateIn : -mateIn);
    } else {
        std::cout << " score cp " << score;
    }
    std::cout << " nodes " << engine.nodesSearched
              << " pv "     << moveToUci(best) << "\n";
    std::cout << "bestmove " << moveToUci(best) << "\n";
}

int main() {
    std::ios::sync_with_stdio(false);

    Board board;
    board.setStartPosition();
    MoveGenerator gen;
    Engine engine;
    engine.verbose = false; // keep stdout clean for protocol traffic

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name ZeroFish\n";
            std::cout << "id author yusuferoglu52\n";
            std::cout << "uciok\n";
        } else if (cmd == "isready") {
            std::cout << "readyok\n";
        } else if (cmd == "ucinewgame") {
            board.setStartPosition();
        } else if (cmd == "position") {
            parsePosition(ss, board, gen);
        } else if (cmd == "go") {
            handleGo(ss, board, engine);
        } else if (cmd == "stop") {
            // synchronous search: nothing in flight to interrupt
        } else if (cmd == "quit") {
            break;
        }
        // unknown commands (setoption, debug, ponderhit, ...) are ignored

        std::cout.flush();
    }
    return 0;
}
