#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include "Board.h"
#include "Engine.h"
#include "MoveGenerator.h"

struct SuitePos {
    const char* fen;
    const char* name;
    const char* expected; // optional ("" => no strict regression)
};

static std::string moveName(const Move& m) {
    std::string s;
    s += (char)('a' + ::file(m.from));
    s += (char)('1' + rank(m.from));
    s += (char)('a' + ::file(m.to));
    s += (char)('1' + rank(m.to));
    if (m.promoted != NO_PIECE) s += "pnbrqk"[m.promoted];
    return s;
}

static const SuitePos kBenchSuite[] = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "Start", ""},
    {"r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", "Italian", ""},
    {"r2q1rk1/pp2bppp/2n1pn2/2bp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 w - - 0 10", "Middlegame 1", ""},
    {"2r2rk1/1bqnbppp/p3pn2/1p2N3/3P4/1BN1P3/PPQ2PPP/2RR2K1 w - - 0 18", "Middlegame 2", ""},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", "Rook Endgame", ""}
};

static const SuitePos kRegressionSuite[] = {
    {"6k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1", "Quiet", ""},
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "Kiwipete", ""},
    {"r1bq1rk1/pp1n1ppp/2pbpn2/3p4/3P4/2NBPN2/PPQ2PPP/R1B2RK1 w - - 0 9", "Strategic", ""},
    {"4rrk1/pp1n1ppp/2p1pn2/3p4/3P4/2N1PN2/PP3PPP/R2R2K1 w - - 0 14", "Middlegame tension", ""}
};

static void runBench(Engine& engine, int timeMs, long long nodes) {
    long long totalNodes = 0;
    double totalMs = 0.0;
    std::cout << "== Bench (" << timeMs << " ms, " << nodes << " nodes) ==\n";
    for (const auto& p : kBenchSuite) {
        Board b;
        b.parseFEN(p.fen);
        auto t0 = std::chrono::high_resolution_clock::now();
        Move best = engine.findBestMove(b, 0, timeMs, nodes);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        totalMs += ms;
        totalNodes += engine.nodesSearched;
        std::cout << p.name << ": " << moveName(best)
                  << " | d=" << engine.maxDepthReached
                  << " | " << (long long)ms << " ms"
                  << " | " << engine.nodesSearched << " nodes\n";
    }
    std::cout << "Total: " << (long long)totalMs << " ms, " << totalNodes << " nodes\n";
}

static int runRegression(Engine& engine, int timeMs, long long nodes) {
    int solved = 0;
    std::cout << "== Regression (" << timeMs << " ms, " << nodes << " nodes) ==\n";
    for (const auto& p : kRegressionSuite) {
        Board b;
        b.parseFEN(p.fen);
        Move best = engine.findBestMove(b, 0, timeMs, nodes);
        std::string got = moveName(best);
        bool pass = std::strlen(p.expected) == 0 || got == p.expected;
        solved += pass ? 1 : 0;
        std::cout << (pass ? "[OK] " : "[XX] ") << p.name << " -> " << got;
        if (std::strlen(p.expected) != 0) std::cout << " (exp " << p.expected << ")";
        std::cout << "\n";
    }
    std::cout << "Regression score: " << solved << "/" << (int)(sizeof(kRegressionSuite)/sizeof(kRegressionSuite[0])) << "\n";
    return solved;
}

static void runTuning(Engine& engine, int timeMs, long long nodes) {
    std::vector<Engine::SearchTuning> presets = {
        {35, 200, 380, 2.25},
        {28, 180, 340, 2.15},
        {42, 220, 420, 2.35},
        {32, 190, 360, 2.05},
        {38, 210, 400, 2.45}
    };

    int bestIdx = 0;
    long long bestScore = -9e18;
    std::cout << "== Tuning loop ==\n";
    for (int i = 0; i < (int)presets.size(); i++) {
        engine.setSearchTuning(presets[i]);
        auto t0 = std::chrono::high_resolution_clock::now();
        int reg = runRegression(engine, timeMs, nodes);
        runBench(engine, timeMs, nodes);
        auto t1 = std::chrono::high_resolution_clock::now();
        long long elapsed = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        long long score = reg * 100000LL - elapsed;
        std::cout << "Preset #" << i
                  << " => score=" << score
                  << " (null=" << presets[i].nullMargin
                  << ", fut=" << presets[i].futilityD1 << "/" << presets[i].futilityD2
                  << ", lmrDiv=" << presets[i].lmrDivisor << ")\n";
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    engine.setSearchTuning(presets[bestIdx]);
    std::cout << "Best preset: #" << bestIdx
              << " (null=" << presets[bestIdx].nullMargin
              << ", fut=" << presets[bestIdx].futilityD1 << "/" << presets[bestIdx].futilityD2
              << ", lmrDiv=" << presets[bestIdx].lmrDivisor << ")\n";
}

static void validateBook(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cout << "[ERR] Cannot open book file: " << path << "\n";
        return;
    }

    MoveGenerator gen;
    std::unordered_set<std::string> seen;
    int lineNo = 0;
    int ok = 0;
    int badFormat = 0;
    int badMove = 0;
    int tooShort = 0;
    int duplicates = 0;

    std::string line;
    while (std::getline(in, line)) {
        lineNo++;
        if (line.empty() || line[0] == '#') continue;

        size_t p1 = line.find('|');
        size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find('|', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) {
            badFormat++;
            std::cout << "[FORMAT] line " << lineNo << "\n";
            continue;
        }

        std::string opening = line.substr(0, p1);
        std::string variation = line.substr(p1 + 1, p2 - p1 - 1);
        std::string movesStr = line.substr(p2 + 1);
        if (opening.empty() || variation.empty() || movesStr.empty()) {
            badFormat++;
            std::cout << "[FORMAT] line " << lineNo << "\n";
            continue;
        }

        std::istringstream ss(movesStr);
        std::vector<std::string> moves;
        std::string tok;
        while (ss >> tok) moves.push_back(tok);
        if (moves.size() < 20) {
            tooShort++;
            std::cout << "[SHORT] line " << lineNo << " (" << moves.size() << " ply)\n";
            continue;
        }

        std::string key = opening + "|" + variation + "|" + movesStr;
        if (!seen.insert(key).second) {
            duplicates++;
            std::cout << "[DUP] line " << lineNo << "\n";
            continue;
        }

        Board b;
        b.setStartPosition();
        bool legalLine = true;
        for (size_t i = 0; i < moves.size(); i++) {
            auto legal = gen.generateMoves(b);
            bool found = false;
            for (const auto& m : legal) {
                if (moveName(m) == moves[i]) {
                    b.makeMove(m);
                    found = true;
                    break;
                }
            }
            if (!found) {
                badMove++;
                std::cout << "[ILLEGAL] line " << lineNo << " ply " << (i + 1)
                          << " move " << moves[i] << "\n";
                legalLine = false;
                break;
            }
        }
        if (legalLine) ok++;
    }

    int totalIssues = badFormat + badMove + tooShort + duplicates;
    std::cout << "\n== Book validation report ==\n";
    std::cout << "Valid lines: " << ok << "\n";
    std::cout << "Format errors: " << badFormat << "\n";
    std::cout << "Illegal lines: " << badMove << "\n";
    std::cout << "Too short (<20 ply): " << tooShort << "\n";
    std::cout << "Duplicates: " << duplicates << "\n";
    std::cout << "Total issues: " << totalIssues << "\n";
}

int main(int argc, char** argv) {
    Engine engine;
    engine.verbose = false;

    bool doBench = false, doReg = false, doTune = false, doValidateBook = false;
    int timeMs = 1200;
    long long nodes = 600000;
    int depth = 0; // 0 => auto
    std::string bookPath = "assets/openings.book";
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--bench") == 0) doBench = true;
        else if (std::strcmp(argv[i], "--regress") == 0) doReg = true;
        else if (std::strcmp(argv[i], "--tune") == 0) doTune = true;
        else if (std::strcmp(argv[i], "--validate-book") == 0) doValidateBook = true;
        else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            timeMs = std::max(0, std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            nodes = std::max(0LL, std::stoll(argv[++i]));
        } else if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
            depth = std::max(0, std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--book-file") == 0 && i + 1 < argc) {
            bookPath = argv[++i];
        }
    }

    if (!doBench && !doReg && !doTune && !doValidateBook) {
        std::cout << "Usage: chess_engine [--bench] [--regress] [--tune] [--validate-book]"
                  << " [--time ms] [--nodes n] [--depth d] [--book-file path]\n";
        std::cout << "Default quick run:\n";
        runBench(engine, 1000, 500000);
        return 0;
    }

    if (doValidateBook) {
        validateBook(bookPath);
        if (!doBench && !doReg && !doTune) return 0;
    }

    if (depth > 0) {
        // Force fixed depth while keeping time/nodes if caller wants.
        // Implemented by overriding suite runners inline.
        std::cout << "Fixed depth mode: " << depth << "\n";
        for (const auto& p : kBenchSuite) {
            Board b;
            b.parseFEN(p.fen);
            Move best = engine.findBestMove(b, depth, timeMs, nodes);
            std::cout << p.name << ": " << moveName(best)
                      << " | d=" << engine.maxDepthReached
                      << " | " << engine.nodesSearched << " nodes\n";
        }
        if (doReg) {
            std::cout << "Regression (fixed depth)\n";
            for (const auto& p : kRegressionSuite) {
                Board b;
                b.parseFEN(p.fen);
                Move best = engine.findBestMove(b, depth, timeMs, nodes);
                std::cout << p.name << " -> " << moveName(best) << "\n";
            }
        }
        if (doTune) {
            std::cout << "--tune ignores fixed depth and uses suite defaults.\n";
            runTuning(engine, timeMs, nodes);
        }
        return 0;
    }

    if (doBench) runBench(engine, timeMs, nodes);
    if (doReg) runRegression(engine, timeMs, nodes);
    if (doTune) runTuning(engine, timeMs, nodes);
    return 0;
}
