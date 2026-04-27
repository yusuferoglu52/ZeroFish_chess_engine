#include "ChessApp.h"
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>

// ---- Color palette ----
static const sf::Color LIGHT_SQ  = {240, 217, 181};
static const sf::Color DARK_SQ   = {181, 136,  99};
static const sf::Color SEL_COL   = { 20, 160,  20, 180};
static const sf::Color HINT_COL  = { 20, 160,  20, 130};
static const sf::Color CHECK_COL = {220,  40,  40, 190};
static const sf::Color LASTMV_COL= {205, 210,  56, 150}; // yellow last-move
static const sf::Color PANEL_BG  = { 28,  28,  36};
static const sf::Color BAR_COL   = { 33,  33,  42};
static const sf::Color TEXT_COL  = {220, 220, 220};
static const sf::Color DIM_COL   = {120, 120, 132};
static const sf::Color BTN_NORM  = { 52,  52,  68};
static const sf::Color BTN_HOT   = { 65, 130, 195};
static const sf::Color TOPBAR_BG = { 36,  36,  48};

static const char* COLOR_PFX[2] = {"w","b"};
static const char  PIECE_CHR[6] = {'P','N','B','R','Q','K'};

struct BookLine {
    std::string opening;
    std::string variation;
    std::vector<std::string> moves;
};

static const std::vector<BookLine> DEFAULT_OPENING_BOOK = {
    {"Ruy Lopez", "Morphy Defense - Closed", {
        "e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1","f8e7",
        "f1e1","b7b5","a4b3","d7d6","c2c3","e8g8","h2h3","c6b8","d2d4","b8d7",
        "b1d2","c8b7","b3c2","f8e8"
    }},
    {"Italian Game", "Giuoco Piano - Main Line", {
        "e2e4","e7e5","g1f3","b8c6","f1c4","f8c5","c2c3","g8f6","d2d3","d7d6",
        "e1g1","e8g8","f1e1","a7a6","c4b3","c5a7","h2h3","h7h6","b1d2","c8e6",
        "b3c2","d8d7","d2f1","f8e8"
    }},
    {"Sicilian Defense", "Najdorf - English Attack", {
        "e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","a7a6",
        "c1e3","e7e5","d4b3","c8e6","f2f3","h7h5","d1d2","b8d7","e1c1","h5h4",
        "g2g4","f6b6","g4g5","f8e7"
    }},
    {"French Defense", "Classical Steinitz", {
        "e2e4","e7e6","d2d4","d7d5","b1c3","g8f6","e4e5","f6d7","f2f4","c7c5",
        "g1f3","b8c6","c1e3","c5d4","f3d4","d8b6","d1d2","f8c5","e1c1","e8g8",
        "c3a4","b6d8","a4c5","d7c5"
    }},
    {"Caro-Kann Defense", "Classical Main Line", {
        "e2e4","c7c6","d2d4","d7d5","b1c3","d5e4","c3e4","c8f5","e4g3","f5g6",
        "h2h4","h7h6","g1f3","b8d7","h4h5","g6h7","f1d3","h7d3","d1d3","e7e6",
        "c1d2","g8f6","e1c1","f8e7"
    }},
    {"Queen's Gambit", "Declined Orthodox", {
        "d2d4","d7d5","c2c4","e7e6","b1c3","g8f6","c1g5","f8e7","e2e3","e8g8",
        "g1f3","h7h6","g5h4","b7b6","f1d3","c8b7","e1g1","b8d7","d1e2","f6e4",
        "h4e7","d8e7","c4d5","e6d5"
    }},
    {"King's Indian Defense", "Classical Mar del Plata", {
        "d2d4","g8f6","c2c4","g7g6","b1c3","f8g7","e2e4","d7d6","g1f3","e8g8",
        "f1e2","e7e5","e1g1","b8c6","d4d5","c6e7","f3e1","f6d7","f2f3","f7f5",
        "c1e3","f5f4","e3f2","g6g5"
    }},
    {"English Opening", "Symmetrical Four Knights", {
        "c2c4","c7c5","g1f3","g8f6","b1c3","b8c6","g2g3","d7d5","c4d5","f6d5",
        "f1g2","d5c7","e1g1","e7e5","d2d3","f8e7","c1e3","e8g8","a1c1","c8e6",
        "c3e4","b7b6","a2a3","f7f6"
    }}
};
static std::vector<BookLine> OPENING_BOOK = DEFAULT_OPENING_BOOK;

static std::vector<BookLine> loadOpeningBookFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return {};

    std::vector<BookLine> book;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t p1 = line.find('|');
        size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find('|', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) continue;

        BookLine bl;
        bl.opening = line.substr(0, p1);
        bl.variation = line.substr(p1 + 1, p2 - p1 - 1);

        std::istringstream ss(line.substr(p2 + 1));
        std::string mv;
        while (ss >> mv) {
            if (mv.size() >= 4) bl.moves.push_back(mv);
        }
        if ((int)bl.moves.size() >= 20) book.push_back(std::move(bl));
    }
    return book;
}

static int maxBookFullMoves() {
    int maxPlies = 0;
    for (const auto& line : OPENING_BOOK) {
        maxPlies = std::max(maxPlies, (int)line.moves.size());
    }
    if (maxPlies <= 0) return 40; // fallback when no book lines exist
    return std::max(1, maxPlies / 2);
}

// ---- Constructor ----
ChessApp::ChessApp()
    : window(sf::VideoMode({(unsigned)W,(unsigned)H}), "Chess Engine",
             sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize)
{
    window.setFramerateLimit(60);
    window.setSize({(unsigned)W, (unsigned)H});
    uiView = sf::View(sf::FloatRect({0.f, 0.f}, {(float)W, (float)H}));
    updateViewForWindowSize(window.getSize());
    window.setView(uiView);

    if (!font.openFromFile("C:/Windows/Fonts/segoeui.ttf"))
        if (!font.openFromFile("C:/Windows/Fonts/arial.ttf"))
            [[maybe_unused]] bool _ = font.openFromFile("C:/Windows/Fonts/verdana.ttf");

    namespace fs = std::filesystem;
    texturesLoaded = true;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            std::string fn = std::string("assets/pieces/") + COLOR_PFX[c] + PIECE_CHR[p] + ".png";
            if (!pieceTextures[c][p].loadFromFile(fn)) texturesLoaded = false;
        }

    engine.verbose = false;
    auto loadedBook = loadOpeningBookFromFile("assets/openings.book");
    if (!loadedBook.empty()) OPENING_BOOK = std::move(loadedBook);
    int maxMoves = maxBookFullMoves();
    bookMaxPly = std::clamp(bookMaxPly, 0, maxMoves + 1);
    board.setStartPosition();
    buildDepthButtons();
    updateEval();
    refreshOpeningLabel();
    updateStatus();
}

ChessApp::~ChessApp() {
    engineBusy = false;
    if (engineThread.joinable()) engineThread.join();
}

// ---- Depth buttons ----
void ChessApp::buildDepthButtons() {
    depthButtons.clear();
    const int depths[] = {0, 1, 3, 5, 7};
    float bx  = EVAL_W + BW + 12;
    float by  = TOP + 50;
    float bw  = SIDE - 24;
    float bh  = 28;
    float gap = 6;
    for (int d : depths) {
        Button b;
        b.rect  = sf::FloatRect({bx, by}, {bw, bh});
        b.label = (d == 0) ? "Depth Auto" : ("Depth " + std::to_string(d));
        b.value = d;
        depthButtons.push_back(b);
        by += bh + gap;
    }

    by += 6;
    bookToggleButton.rect = sf::FloatRect({bx, by}, {bw, bh});
    bookToggleButton.label = "BOOK MOVES";
    bookToggleButton.value = 1;

    // Keep extra breathing room so BOOK MOVES state text never overlaps
    // with the Book max ply label/slider on smaller window sizes.
    by += 52;
    bookPlySliderRect = sf::FloatRect({bx, by}, {bw, 10.f});

    by += 46;
    timeSliderRect = sf::FloatRect({bx, by}, {bw, 10.f});
    by += 38;
    nodeSliderRect = sf::FloatRect({bx, by}, {bw, 10.f});

    by += 18;
    startFromHereButton.rect = sf::FloatRect({bx, by}, {bw, 28.f});
    startFromHereButton.label = "Start from here";
    startFromHereButton.value = 1;
}

// ---- Main loop ----
void ChessApp::run() {
    while (window.isOpen()) {
        handleEvents();
        update();
        render();
    }
    if (engineThread.joinable()) engineThread.join();
}

void ChessApp::handleEvents() {
    while (const auto event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>())
            window.close();
        else if (const auto* rs = event->getIf<sf::Event::Resized>()) {
            updateViewForWindowSize(rs->size);
            window.setView(uiView);
        }

        else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mb->button == sf::Mouse::Button::Left) {
                sf::Vector2f mp = window.mapPixelToCoords(mb->position, uiView);
                if (timeSliderRect.contains(mp)) {
                    draggingTimeSlider = true;
                    updateTimeFromSlider(mp.x);
                    continue;
                }
                if (bookPlySliderRect.contains(mp)) {
                    draggingBookPlySlider = true;
                    updateBookPlyFromSlider(mp.x);
                    continue;
                }
                if (nodeSliderRect.contains(mp)) {
                    draggingNodeSlider = true;
                    updateNodesFromSlider(mp.x);
                    continue;
                }
                onClick({(int)mp.x, (int)mp.y});
            }
        }
        else if (const auto* mm = event->getIf<sf::Event::MouseMoved>()) {
            sf::Vector2f mp = window.mapPixelToCoords(mm->position, uiView);
            if (draggingTimeSlider) updateTimeFromSlider(mp.x);
            if (draggingBookPlySlider) updateBookPlyFromSlider(mp.x);
            if (draggingNodeSlider) updateNodesFromSlider(mp.x);
            promotionHoverIndex = promotionPending ? promotionChoiceAt(mp) : -1;
        }
        else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mr->button == sf::Mouse::Button::Left) {
                draggingTimeSlider = false;
                draggingBookPlySlider = false;
                draggingNodeSlider = false;
            }
        }
        else if (const auto* mw = event->getIf<sf::Event::MouseWheelScrolled>()) {
            // Scroll move history with mouse wheel over right panel
            if (mw->position.x > EVAL_W + BW) {
                historyScroll -= (int)mw->delta;
                int pairs = ((int)moveHistory.size() + 1) / 2;
                historyScroll = std::clamp(historyScroll, 0, std::max(0, pairs - 4));
            }
        }
        else if (const auto* kb = event->getIf<sf::Event::KeyPressed>()) {
            if (promotionPending) {
                if (kb->code == sf::Keyboard::Key::Q) { choosePromotion(QUEEN); continue; }
                if (kb->code == sf::Keyboard::Key::R) { choosePromotion(ROOK);  continue; }
                if (kb->code == sf::Keyboard::Key::B) { choosePromotion(BISHOP);continue; }
                if (kb->code == sf::Keyboard::Key::N) { choosePromotion(KNIGHT);continue; }
            }
            if (kb->code == sf::Keyboard::Key::R) {
                if (engineThread.joinable()) engineThread.join();
                board.setStartPosition();
                selectedSq = -1; candidates.clear();
                gameOver = false; pendingMove.reset();
                engineBusy = false; lastMove.reset();
                promotionPending = false; promotionMoves.clear();
                bookMovesLocked = false;
                bookMovesEnabled = true;
                playedMoves.clear(); currentPly = 0;
                moveHistory.clear(); historyScroll = 0;
                updateEval(); refreshOpeningLabel(); updateStatus();
            }
            if (kb->code == sf::Keyboard::Key::F) {
                boardFlipped = !boardFlipped;
                selectedSq = -1;
                candidates.clear();
                updateStatus();
            }
            if (kb->code == sf::Keyboard::Key::C) {
                humanColor = (humanColor == WHITE) ? BLACK : WHITE;
                selectedSq = -1;
                candidates.clear();
                updateStatus();
            }
            if (kb->code == sf::Keyboard::Key::Left && !engineBusy) {
                if (currentPly > 0) {
                    board.unmakeMove(playedMoves[currentPly - 1]);
                    currentPly--;
                    selectedSq = -1; candidates.clear();
                    pendingMove.reset();
                    gameOver = false;
                    promotionPending = false; promotionMoves.clear();
                    lastMove = (currentPly > 0) ? std::optional<Move>(playedMoves[currentPly - 1])
                                                : std::nullopt;
                    checkGameOver();
                    updateEval();
                    refreshOpeningLabel();
                    updateStatus();
                }
            }
            if (kb->code == sf::Keyboard::Key::Right && !engineBusy) {
                if (currentPly < (int)playedMoves.size()) {
                    const Move& m = playedMoves[currentPly];
                    board.makeMove(m);
                    currentPly++;
                    selectedSq = -1; candidates.clear();
                    pendingMove.reset();
                    gameOver = false;
                    promotionPending = false; promotionMoves.clear();
                    lastMove = m;
                    checkGameOver();
                    updateEval();
                    refreshOpeningLabel();
                    updateStatus();
                }
            }
        }
    }
}

void ChessApp::update() {
    if (!engineBusy) {
        std::lock_guard<std::mutex> lk(moveMutex);
        if (pendingMove.has_value() && !gameOver) {
            applyMove(*pendingMove);
            pendingMove.reset();
            checkGameOver();
            updateStatus();
        }
    }
    if (!isHumanTurn() && !engineBusy && !pendingMove.has_value() && !gameOver && isLivePosition())
        startEngine();
}

// ---- Input ----
void ChessApp::onClick(sf::Vector2i pos) {
    if (promotionPending) { handlePromotionClick(pos); return; }

    // Depth buttons
    for (auto& b : depthButtons)
        if (b.rect.contains({(float)pos.x,(float)pos.y})) {
            onDepthButton(b.value); return;
        }
    if (bookToggleButton.rect.contains({(float)pos.x, (float)pos.y})) {
        onBookToggleButton(); return;
    }
    if (startFromHereButton.rect.contains({(float)pos.x, (float)pos.y})) {
        onStartFromHereButton(); return;
    }
    if (!isHumanTurn() || gameOver || engineBusy || !isLivePosition()) return;
    int clicked = posToSq(pos);
    if (clicked < 0) return;

    if (selectedSq == -1) {
        if (board.colorOn[clicked] == (uint8_t)humanColor) {
            selectedSq = clicked;
            auto all = moveGen.generateMoves(board);
            candidates.clear();
            for (auto& m : all) if (m.from == clicked) candidates.push_back(m);
        }
    } else {
        bool moved = false;
        for (auto& m : candidates) {
            if (m.to == clicked) {
                std::vector<Move> promos;
                for (const auto& p : candidates)
                    if (p.to == clicked && p.promoted != NO_PIECE) promos.push_back(p);
                if (!promos.empty()) {
                    promotionPending = true;
                    promotionMoves = promos;
                    statusMsg = "Choose promotion: Q/R/B/N";
                } else {
                    applyMove(m);
                    checkGameOver(); updateStatus();
                }
                selectedSq = -1; candidates.clear();
                moved = true; break;
            }
        }
        if (!moved) {
            if (board.colorOn[clicked] == (uint8_t)humanColor) {
                selectedSq = clicked;
                auto all = moveGen.generateMoves(board);
                candidates.clear();
                for (auto& m : all) if (m.from == clicked) candidates.push_back(m);
            } else {
                selectedSq = -1; candidates.clear();
            }
        }
    }
}

void ChessApp::onDepthButton(int d) { engineDepth = std::max(0, d); }
void ChessApp::onBookToggleButton() {
    if (bookMovesLocked) return;
    bookMovesEnabled = !bookMovesEnabled;
}
void ChessApp::onStartFromHereButton() {
    if (engineBusy || isLivePosition()) return;
    playedMoves.resize(currentPly);
    moveHistory.resize(currentPly);
    historyScroll = std::max(0, (((int)moveHistory.size() + 1) / 2) - 4);
    pendingMove.reset();
    gameOver = false;
    promotionPending = false;
    promotionMoves.clear();
    lastMove = (currentPly > 0) ? std::optional<Move>(playedMoves[currentPly - 1]) : std::nullopt;
    bookMovesLocked = false;
    refreshOpeningLabel();
    updateEval();
    updateStatus();
}
void ChessApp::updateBookPlyFromSlider(float x) {
    float t = (x - bookPlySliderRect.position.x) / bookPlySliderRect.size.x;
    t = std::clamp(t, 0.f, 1.f);
    int maxMoves = maxBookFullMoves();
    // Extra slot at far right: after N/N, show "max".
    int raw = BOOK_PLY_MIN + (int)std::lround(t * ((maxMoves + 1) - BOOK_PLY_MIN));
    bookMaxPly = std::clamp(raw, BOOK_PLY_MIN, maxMoves + 1);
}
void ChessApp::updateTimeFromSlider(float x) {
    float t = (x - timeSliderRect.position.x) / timeSliderRect.size.x;
    t = std::clamp(t, 0.f, 1.f);
    int raw = TIME_MIN_MS + (int)std::lround(t * (TIME_MAX_MS - TIME_MIN_MS));
    engineTimeMsLimit = (raw / TIME_STEP_MS) * TIME_STEP_MS;
    engineTimeMsLimit = std::clamp(engineTimeMsLimit, TIME_MIN_MS, TIME_MAX_MS);
}
void ChessApp::updateNodesFromSlider(float x) {
    float t = (x - nodeSliderRect.position.x) / nodeSliderRect.size.x;
    t = std::clamp(t, 0.f, 1.f);
    int raw = NODE_MIN + (int)std::lround(t * (NODE_MAX - NODE_MIN));
    engineNodeLimit = (raw / NODE_STEP) * NODE_STEP;
    engineNodeLimit = std::clamp(engineNodeLimit, NODE_MIN, NODE_MAX);
}

// ---- Game logic ----
void ChessApp::applyMove(const Move& m) {
    if (!isLivePosition()) {
        playedMoves.resize(currentPly);
        moveHistory.resize(currentPly);
    }

    std::string notation = moveStr(m);
    board.makeMove(m);
    playedMoves.push_back(m);
    currentPly++;
    lastMove = m;

    // Append + or # after applying
    if (moveGen.isInCheck(board, board.sideToMove)) {
        auto replies = moveGen.generateMoves(board);
        notation += replies.empty() ? "#" : "+";
    }
    moveHistory.push_back(notation);
    updateEval();
    refreshOpeningLabel();
    autoScrollHistory();
}

void ChessApp::startEngine() {
    int maxMoves = maxBookFullMoves();
    bool useBookMaxMode = (bookMaxPly > maxMoves);
    // bookMaxPly slider is in full-move units; convert to plies for gating.
    int bookMaxPlies = (bookMaxPly <= 0 || useBookMaxMode) ? 0 : (bookMaxPly * 2);
    bool allowBookByPly = (bookMaxPlies <= 0) || (currentPly < bookMaxPlies);
    if (bookMovesEnabled && allowBookByPly) {
        if (auto m = findBookMove()) {
            std::lock_guard<std::mutex> lk(moveMutex);
            pendingMove = *m;
            engineBusy = false;
            lastEngineMoveFromBook = true;
            statusMsg = "Book move";
            return;
        }
        // Auto-disable book once we leave known lines.
        bookMovesEnabled = false;
        bookMovesLocked = true;
        currentlyOutOfBook = true;
    } else if (bookMovesEnabled && !allowBookByPly) {
        bookMovesEnabled = false;
        currentlyOutOfBook = true;
    }
    lastEngineMoveFromBook = false;
    engineBusy = true;
    statusMsg = "Out of book - engine thinking...";
    Board copy = board;
    const int searchDepth = (engineDepth <= 0) ? 0 : std::max(1, engineDepth);
    const int timeLimit = std::max(0, engineTimeMsLimit);
    const long long nodeLimit = std::max(0, engineNodeLimit);
    if (engineThread.joinable()) engineThread.join();
    engineThread = std::thread([this, copy, searchDepth, timeLimit, nodeLimit]() mutable {
        Move best = engine.findBestMove(copy, searchDepth, timeLimit, nodeLimit);
        std::lock_guard<std::mutex> lk(moveMutex);
        pendingMove = best;
        engineBusy = false;
    });
}

void ChessApp::checkGameOver() {
    if (board.isThreefoldRepetition()) {
        gameOver = true;
        statusMsg = "Draw — threefold repetition.";
        return;
    }
    if (board.isFiftyMoveDraw()) {
        gameOver = true;
        statusMsg = "Draw — fifty-move rule.";
        return;
    }
    if (board.isInsufficientMaterial()) {
        gameOver = true;
        statusMsg = "Draw — insufficient material.";
        return;
    }

    auto moves = moveGen.generateMoves(board);
    if (!moves.empty()) return;
    gameOver = true;
    if (moveGen.isInCheck(board, board.sideToMove))
        statusMsg = (board.sideToMove == BLACK) ? "Checkmate — White wins!"
                                                 : "Checkmate — Black wins!";
    else
        statusMsg = "Stalemate — Draw.";
}

void ChessApp::updateStatus() {
    if (gameOver) return;
    bool chk = moveGen.isInCheck(board, board.sideToMove);
    std::string side = (board.sideToMove == WHITE) ? "White" : "Black";
    statusMsg = (isHumanTurn() ? "Your turn (" + side + ")"
                               : "Engine thinking... (" + side + ")")
              + (chk ? "  CHECK!" : "");
    if (!isLivePosition()) statusMsg += "  [History view]";
}

void ChessApp::updateEval() {
    int raw = evaluator.evaluate(board);
    // evaluate() returns from sideToMove's perspective; convert to absolute white
    whiteEval = (board.sideToMove == WHITE) ? raw : -raw;
}

void ChessApp::autoScrollHistory() {
    float lineH   = 15.f;
    float sepY    = startFromHereButton.rect.position.y + startFromHereButton.rect.size.y + 12.f;
    float histTop = sepY + 24;
    // Keep history clear of the capture-balance/info block.
    float histBot = (float)(TOP + SQ * 8 - 34);
    int   visLines = (int)((histBot - histTop) / lineH);
    int   pairs    = ((int)moveHistory.size() + 1) / 2;
    historyScroll  = std::max(0, pairs - visLines);
}

bool ChessApp::isHumanTurn() const { return board.sideToMove == humanColor; }
bool ChessApp::isLivePosition() const { return currentPly == (int)playedMoves.size(); }
std::string ChessApp::moveToUci(const Move& m) const {
    std::string s;
    s += (char)('a' + ::file(m.from));
    s += (char)('1' + rank(m.from));
    s += (char)('a' + ::file(m.to));
    s += (char)('1' + rank(m.to));
    if (m.promoted != NO_PIECE) s += "pnbrqk"[m.promoted];
    return s;
}

std::optional<Move> ChessApp::findBookMove() {
    std::vector<std::string> hist;
    hist.reserve((size_t)currentPly);
    for (int i = 0; i < currentPly; i++) hist.push_back(moveToUci(playedMoves[i]));

    std::vector<std::string> candidates;
    for (const auto& line : OPENING_BOOK) {
        if ((int)line.moves.size() <= currentPly) continue;
        bool ok = true;
        for (int i = 0; i < currentPly; i++) {
            if (line.moves[(size_t)i] != hist[(size_t)i]) { ok = false; break; }
        }
        if (ok) candidates.push_back(line.moves[(size_t)currentPly]);
    }
    if (candidates.empty()) return std::nullopt;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    Board b = board;
    auto legal = moveGen.generateMoves(b);
    for (const auto& c : candidates) {
        for (const auto& m : legal) {
            if (moveToUci(m) == c) return m;
        }
    }
    return std::nullopt;
}

void ChessApp::refreshOpeningLabel() {
    currentOpeningName.clear();
    currentVariationName.clear();
    currentlyOutOfBook = false;
    std::vector<std::string> hist;
    hist.reserve((size_t)currentPly);
    for (int i = 0; i < currentPly; i++) hist.push_back(moveToUci(playedMoves[i]));

    int bestLen = 0;
    bool hasPrefix = false;
    for (const auto& line : OPENING_BOOK) {
        int match = 0;
        int limit = std::min((int)line.moves.size(), currentPly);
        while (match < limit && line.moves[(size_t)match] == hist[(size_t)match]) match++;
        if (match == currentPly) hasPrefix = true;
        if (match > bestLen && match >= 2) {
            bestLen = match;
            currentOpeningName = line.opening;
            currentVariationName = line.variation;
        }
    }
    if (bookMovesEnabled && currentPly >= 2 && !hasPrefix) currentlyOutOfBook = true;
}
int ChessApp::captureBalance() const {
    static constexpr int START_COUNTS[6] = {8, 2, 2, 2, 1, 1};
    static constexpr int PIECE_VALUES[6] = {1, 3, 3, 5, 9, 0};

    int whiteCaptured = 0;
    int blackCaptured = 0;
    for (int pt = PAWN; pt <= QUEEN; pt++) {
        int whiteAlive = popcount(board.pieces[WHITE][pt]);
        int blackAlive = popcount(board.pieces[BLACK][pt]);
        int whiteMissing = START_COUNTS[pt] - whiteAlive;
        int blackMissing = START_COUNTS[pt] - blackAlive;
        blackCaptured += whiteMissing * PIECE_VALUES[pt];
        whiteCaptured += blackMissing * PIECE_VALUES[pt];
    }
    return whiteCaptured - blackCaptured; // + => white captured more, - => black captured more
}

void ChessApp::choosePromotion(PieceType pt) {
    if (!promotionPending) return;
    for (const auto& m : promotionMoves) {
        if ((PieceType)m.promoted == pt) {
            applyMove(m);
            promotionPending = false;
            promotionMoves.clear();
            checkGameOver();
            updateStatus();
            return;
        }
    }
}

bool ChessApp::handlePromotionClick(sf::Vector2i pos) {
    if (!promotionPending) return false;
    const PieceType order[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
    int idx = promotionChoiceAt({(float)pos.x, (float)pos.y});
    if (idx >= 0 && idx < 4) {
        choosePromotion(order[idx]);
        return true;
    }
    return false;
}

int ChessApp::promotionChoiceAt(sf::Vector2f pos) const {
    float panelW = 320.f, panelH = 128.f;
    float px = EVAL_W + (BW - panelW) * 0.5f;
    float py = TOP + (SQ * 8 - panelH) * 0.5f;
    float by = py + 44.f;
    float bx = px + 12.f;
    float bw = 68.f, bh = 68.f, gap = 8.f;
    for (int i = 0; i < 4; i++) {
        sf::FloatRect r({bx + i * (bw + gap), by}, {bw, bh});
        if (r.contains(pos)) return i;
    }
    return -1;
}

void ChessApp::updateViewForWindowSize(sf::Vector2u size) {
    float ww = std::max(1u, size.x);
    float wh = std::max(1u, size.y);
    float targetRatio = (float)W / (float)H;
    float windowRatio = ww / wh;
    float viewportW = 1.f, viewportH = 1.f, viewportX = 0.f, viewportY = 0.f;
    if (windowRatio > targetRatio) {
        viewportW = targetRatio / windowRatio;
        viewportX = (1.f - viewportW) * 0.5f;
    } else if (windowRatio < targetRatio) {
        viewportH = windowRatio / targetRatio;
        viewportY = (1.f - viewportH) * 0.5f;
    }
    uiView.setSize({(float)W, (float)H});
    uiView.setCenter({W * 0.5f, H * 0.5f});
    uiView.setViewport({{viewportX, viewportY}, {viewportW, viewportH}});
}

// ---- Coordinates ----
int ChessApp::posToSq(sf::Vector2i pos) const {
    int col = (pos.x - EVAL_W) / SQ;
    int row = (pos.y - TOP) / SQ;
    if (col < 0 || col > 7 || row < 0 || row > 7) return -1;
    if (boardFlipped) {
        col = 7 - col;
        row = 7 - row;
    }
    return col + (7 - row) * 8;
}

sf::Vector2f ChessApp::sqToPos(int s) const {
    int col = s % 8;
    int row = 7 - s / 8;
    if (boardFlipped) {
        col = 7 - col;
        row = 7 - row;
    }
    return { (float)(EVAL_W + col * SQ), (float)(TOP + row * SQ) };
}

sf::Color ChessApp::squareColor(int s) const {
    return ((s / 8 + s % 8) % 2 == 0) ? DARK_SQ : LIGHT_SQ;
}

// ---- Move notation (LAN) ----
std::string ChessApp::moveStr(const Move& m) {
    if (m.castling) return (::file(m.to) == 6) ? "O-O" : "O-O-O";
    std::string s;
    if ((PieceType)m.piece != PAWN) s += PIECE_CHR[m.piece];
    s += (char)('a' + ::file(m.from));
    s += (char)('1' + rank(m.from));
    if (m.captured != NO_PIECE || m.enPassant) s += 'x';
    s += (char)('a' + ::file(m.to));
    s += (char)('1' + rank(m.to));
    if (m.promoted != NO_PIECE) { s += '='; s += PIECE_CHR[m.promoted]; }
    return s;
}

// ======== RENDERING ========

void ChessApp::render() {
    window.setView(uiView);
    window.clear({18, 18, 24});
    drawTopBar();
    drawEvalBar();
    drawBoard();
    drawCoordinates();
    drawHighlights();
    drawPieces();
    if (promotionPending) drawPromotionOverlay();
    drawStatusBar();
    drawSidePanel();
    window.display();
}

// ---- Top bar ----
void ChessApp::drawTopBar() {
    sf::RectangleShape bg({(float)W,(float)TOP});
    bg.setPosition({0,0});
    bg.setFillColor(TOPBAR_BG);
    window.draw(bg);

    sf::Text title(font, "Chess Engine", 21);
    title.setStyle(sf::Text::Bold);
    title.setFillColor({205,205,220});
    title.setPosition({(float)(EVAL_W + 10), 13});
    window.draw(title);

    sf::Text hint(font, "[R] New game [F] Flip [C] Side [<-][->] History", 12);
    hint.setFillColor(DIM_COL);
    auto hb = hint.getLocalBounds();
    hint.setPosition({(float)(EVAL_W + BW) - hb.size.x - 10, 17});
    window.draw(hint);

    if (!currentOpeningName.empty()) {
        std::string op = currentOpeningName + " - " + currentVariationName;
        sf::Text opTxt(font, op, 12);
        opTxt.setFillColor({160, 195, 255});
        opTxt.setPosition({(float)(EVAL_W + 12), 34});
        window.draw(opTxt);
    } else if (currentlyOutOfBook) {
        sf::Text opTxt(font, "Out of book", 12);
        opTxt.setFillColor({255, 170, 170});
        opTxt.setPosition({(float)(EVAL_W + 12), 34});
        window.draw(opTxt);
    }
}

// ---- Eval bar ----
void ChessApp::drawEvalBar() {
    float bx = 0, by = (float)TOP;
    float bw = (float)EVAL_W, bh = (float)(SQ * 8);

    // Clamp to ±700 cp for display
    float clamped   = (float)std::clamp(whiteEval, -700, 700);
    float whiteFrac = 0.5f + clamped / 1400.f;
    float blackH    = bh * (1.f - whiteFrac);
    float whiteH    = bh * whiteFrac;

    // Black section (top)
    sf::RectangleShape blk({bw, blackH});
    blk.setPosition({bx, by});
    blk.setFillColor({30,30,30});
    window.draw(blk);

    // White section (bottom)
    sf::RectangleShape wht({bw, whiteH});
    wht.setPosition({bx, by + blackH});
    wht.setFillColor({215,215,215});
    window.draw(wht);

    // Thin midline marker
    float midY = by + bh * 0.5f;
    sf::RectangleShape mid({bw, 1});
    mid.setPosition({bx, midY});
    mid.setFillColor({90,90,90});
    window.draw(mid);

    // Border
    sf::RectangleShape border({bw, bh});
    border.setPosition({bx, by});
    border.setFillColor({0,0,0,0});
    border.setOutlineColor({60,60,72});
    border.setOutlineThickness(1);
    window.draw(border);

    // Eval score text — just above bar (in top bar row)
    std::string evalStr;
    if (std::abs(whiteEval) >= 9000) {
        evalStr = (whiteEval > 0) ? "W+" : "B+";
    } else {
        float v = std::abs(whiteEval) / 100.f;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        evalStr = (whiteEval > 0) ? "+" : (whiteEval < 0 ? "-" : " ");
        if (whiteEval == 0) evalStr = "=";
        else evalStr += buf;
    }
    sf::Text etxt(font, evalStr, 10);
    etxt.setFillColor({180,180,180});
    auto eb = etxt.getLocalBounds();
    etxt.setPosition({bx + bw/2.f - eb.size.x/2.f - eb.position.x, 16});
    window.draw(etxt);
}

// ---- Board ----
void ChessApp::drawBoard() {
    sf::RectangleShape sq({(float)SQ,(float)SQ});
    for (int s = 0; s < 64; s++) {
        sq.setPosition(sqToPos(s));
        sq.setFillColor(squareColor(s));
        window.draw(sq);
    }
}

void ChessApp::drawCoordinates() {
    for (int i = 0; i < 8; i++) {
        int displayRank = boardFlipped ? (8 - i) : (i + 1);
        int rankIndex   = displayRank - 1;
        int rSq = rankIndex * 8; // a-file squares
        sf::Color rc = (squareColor(rSq) == LIGHT_SQ) ? DARK_SQ : LIGHT_SQ;
        sf::Text rt(font, std::to_string(displayRank), 11);
        rt.setFillColor(rc);
        auto rp = sqToPos(rSq);
        rt.setPosition({rp.x + 3, rp.y + 3});
        window.draw(rt);

        int fileIndex = boardFlipped ? (7 - i) : i;
        int fSq = fileIndex; // rank-1 squares
        sf::Color fc = (squareColor(fSq) == LIGHT_SQ) ? DARK_SQ : LIGHT_SQ;
        sf::Text ft(font, std::string(1, 'a' + fileIndex), 11);
        ft.setFillColor(fc);
        auto fp = sqToPos(fSq);
        auto fb = ft.getLocalBounds();
        ft.setPosition({fp.x + SQ - fb.size.x - 4, fp.y + SQ - fb.size.y - 6});
        window.draw(ft);
    }
}

// ---- Highlights: last move (yellow) + check (red) + selected (green) + dots ----
void ChessApp::drawHighlights() {
    sf::RectangleShape hi({(float)SQ,(float)SQ});

    // Last move — yellow
    if (lastMove.has_value()) {
        hi.setFillColor(LASTMV_COL);
        hi.setPosition(sqToPos(lastMove->from)); window.draw(hi);
        hi.setPosition(sqToPos(lastMove->to));   window.draw(hi);
    }

    // King in check — red
    if (moveGen.isInCheck(board, board.sideToMove)) {
        hi.setFillColor(CHECK_COL);
        hi.setPosition(sqToPos(board.kingSquare(board.sideToMove)));
        window.draw(hi);
    }

    // Selected square — green
    if (selectedSq != -1) {
        hi.setFillColor(SEL_COL);
        hi.setPosition(sqToPos(selectedSq));
        window.draw(hi);

        float r = SQ * 0.15f;
        sf::CircleShape dot(r);
        dot.setFillColor(HINT_COL);
        for (auto& m : candidates) {
            auto p = sqToPos(m.to);
            if (m.captured != NO_PIECE || m.enPassant) {
                sf::RectangleShape ring({(float)SQ,(float)SQ});
                ring.setPosition(p);
                ring.setFillColor({0,0,0,0});
                ring.setOutlineColor(HINT_COL);
                ring.setOutlineThickness(6);
                window.draw(ring);
            } else {
                dot.setPosition({p.x + SQ/2.f - r, p.y + SQ/2.f - r});
                window.draw(dot);
            }
        }
    }
}

// ---- Pieces ----
void ChessApp::drawPieces() {
    for (int s = 0; s < 64; s++) {
        if (board.colorOn[s] == 255) continue;
        int c = board.colorOn[s], pt = board.pieceOn[s];
        auto pos = sqToPos(s);

        if (texturesLoaded) {
            sf::Sprite sp(pieceTextures[c][pt]);
            auto sz = pieceTextures[c][pt].getSize();
            sp.setScale({(float)SQ/sz.x,(float)SQ/sz.y});
            sp.setPosition(pos);
            window.draw(sp);
        } else {
            sf::CircleShape body(SQ*0.37f);
            body.setFillColor(c==0 ? sf::Color{255,248,225} : sf::Color{55,38,25});
            body.setOutlineColor(c==0 ? sf::Color{160,120,70} : sf::Color{200,165,110});
            body.setOutlineThickness(2.5f);
            body.setPosition({pos.x+SQ*0.13f, pos.y+SQ*0.13f});
            window.draw(body);
            sf::Text lbl(font, std::string(1,PIECE_CHR[pt]), (unsigned)(SQ*0.38f));
            lbl.setStyle(sf::Text::Bold);
            lbl.setFillColor(c==0 ? sf::Color{80,50,20} : sf::Color{230,205,170});
            auto lb = lbl.getLocalBounds();
            lbl.setPosition({pos.x+SQ/2.f-lb.size.x/2.f-lb.position.x,
                             pos.y+SQ/2.f-lb.size.y/2.f-lb.position.y-2});
            window.draw(lbl);
        }
    }
}

// ---- Status bar (below board, left side only) ----
void ChessApp::drawStatusBar() {
    float barY = (float)(TOP + SQ*8);
    float barW = (float)(EVAL_W + BW);

    sf::RectangleShape bg({barW,(float)BOT});
    bg.setPosition({0, barY});
    bg.setFillColor(BAR_COL);
    window.draw(bg);

    sf::RectangleShape line({barW, 2});
    line.setPosition({0, barY});
    line.setFillColor({55,55,68});
    window.draw(line);

    sf::Text status(font, statusMsg, 17);
    status.setFillColor(gameOver ? sf::Color{255,210,80} : TEXT_COL);
    status.setPosition({(float)(EVAL_W+10), barY+12});
    window.draw(status);

    // Eval value in status bar
    std::string evalStr;
    if (std::abs(whiteEval) >= 9000) {
        evalStr = (whiteEval > 0) ? "White wins" : "Black wins";
    } else {
        float v = whiteEval / 100.f;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%+.2f", v);
        evalStr = std::string("Eval: ") + buf;
    }
    sf::Text evalTxt(font, evalStr, 13);
    evalTxt.setFillColor(whiteEval > 30  ? sf::Color{200,225,200} :
                         whiteEval < -30 ? sf::Color{200,180,180} :
                                           sf::Color{180,180,180});
    evalTxt.setPosition({(float)(EVAL_W+10), barY + BOT - 22});
    window.draw(evalTxt);

    sf::Text nodes(font, "Nodes: " + std::to_string(engine.nodesSearched), 13);
    nodes.setFillColor(DIM_COL);
    auto nb = nodes.getLocalBounds();
    nodes.setPosition({barW - nb.size.x - 10, barY + BOT - 22});
    window.draw(nodes);
}

// ---- Right panel ----
void ChessApp::drawButton(const Button& b, bool active) {
    sf::RectangleShape box({b.rect.size.x, b.rect.size.y});
    box.setPosition({b.rect.position.x, b.rect.position.y});
    box.setFillColor(active ? BTN_HOT : BTN_NORM);
    box.setOutlineColor(active ? sf::Color{110,170,235} : sf::Color{65,65,82});
    box.setOutlineThickness(active ? 2 : 1);
    window.draw(box);

    sf::Text lbl(font, b.label, 14);
    lbl.setFillColor({220,220,230});
    auto lb = lbl.getLocalBounds();
    lbl.setPosition({b.rect.position.x + b.rect.size.x/2.f - lb.size.x/2.f - lb.position.x,
                     b.rect.position.y + b.rect.size.y/2.f - lb.size.y/2.f - lb.position.y});
    window.draw(lbl);
}

void ChessApp::drawSidePanel() {
    float px = (float)(EVAL_W + BW); // panel left edge

    // Panel background
    sf::RectangleShape bg({(float)SIDE,(float)H});
    bg.setPosition({px, 0});
    bg.setFillColor(PANEL_BG);
    window.draw(bg);

    sf::RectangleShape div({2,(float)H});
    div.setPosition({px,0});
    div.setFillColor({50,50,64});
    window.draw(div);

    float ix = px + 10; // inner x

    // ---- Depth section ----
    sf::Text dHdr(font, "ENGINE DEPTH", 12);
    dHdr.setStyle(sf::Text::Bold);
    dHdr.setFillColor({160,160,185});
    dHdr.setPosition({ix, (float)(TOP+14)});
    window.draw(dHdr);

    sf::Text dSub(font, "Half-moves (ply)", 11);
    dSub.setFillColor(DIM_COL);
    dSub.setPosition({ix, (float)(TOP+30)});
    window.draw(dSub);

    for (auto& b : depthButtons) drawButton(b, b.value == engineDepth);

    // ---- Search limit section ----
    float limitsHdrY = depthButtons.back().rect.position.y + depthButtons.back().rect.size.y + 12;
    sf::Text lHdr(font, "SEARCH LIMITS", 12);
    lHdr.setStyle(sf::Text::Bold);
    lHdr.setFillColor({160,160,185});
    lHdr.setPosition({ix, limitsHdrY});
    window.draw(lHdr);

    drawButton(bookToggleButton, bookMovesEnabled && !bookMovesLocked);
    if (bookMovesLocked) {
        sf::RectangleShape lockMask({bookToggleButton.rect.size.x, bookToggleButton.rect.size.y});
        lockMask.setPosition(bookToggleButton.rect.position);
        lockMask.setFillColor({30, 30, 30, 140});
        window.draw(lockMask);
    }
    sf::Text bookState(font, bookMovesEnabled ? "ON" : "OFF", 12);
    if (bookMovesLocked) bookState.setFillColor({170,170,170});
    else bookState.setFillColor(bookMovesEnabled ? sf::Color{170, 225, 170} : sf::Color{220, 170, 170});
    auto bb = bookState.getLocalBounds();
    bookState.setPosition({bookToggleButton.rect.position.x + bookToggleButton.rect.size.x - bb.size.x - 10.f,
                           bookToggleButton.rect.position.y + 8.f});
    window.draw(bookState);
    if (bookMovesLocked) {
        sf::Text lockTxt(font, "LOCK", 10);
        lockTxt.setFillColor({190,190,190});
        lockTxt.setPosition({bookToggleButton.rect.position.x + 10.f, bookToggleButton.rect.position.y + 10.f});
        window.draw(lockTxt);
    }

    auto scalarRatio = [](int value, int minValue, int maxValue) -> float {
        if (maxValue <= minValue) return 0.f;
        return std::clamp((float)(value - minValue) / (float)(maxValue - minValue), 0.f, 1.f);
    };
    auto drawSlider = [&](const sf::FloatRect& rect, float t) {
        sf::RectangleShape track({rect.size.x, rect.size.y});
        track.setPosition(rect.position);
        track.setFillColor({58,58,74});
        track.setOutlineColor({78,78,96});
        track.setOutlineThickness(1.f);
        window.draw(track);

        float knobX = rect.position.x + std::clamp(t, 0.f, 1.f) * rect.size.x;
        sf::CircleShape knob(7.f);
        knob.setOrigin({7.f, 7.f});
        knob.setPosition({knobX, rect.position.y + rect.size.y * 0.5f});
        knob.setFillColor(BTN_HOT);
        knob.setOutlineColor({180,220,255});
        knob.setOutlineThickness(1.f);
        window.draw(knob);
    };

    int bookMaxMoves = maxBookFullMoves();
    int sliderTop = bookMaxMoves + 1; // one extra "max" slot
    if (bookMaxPly > sliderTop) bookMaxPly = sliderTop;
    sf::Text bookPlyLbl(font, "max book move", 11);
    bookPlyLbl.setFillColor(DIM_COL);
    bookPlyLbl.setPosition({ix, bookPlySliderRect.position.y - 18});
    window.draw(bookPlyLbl);
    drawSlider(bookPlySliderRect, scalarRatio(bookMaxPly, BOOK_PLY_MIN, sliderTop));
    std::string bookPlyText;
    if (bookMaxPly == 0) bookPlyText = "off";
    else if (bookMaxMoves > 0 && bookMaxPly > bookMaxMoves) bookPlyText = "max";
    else bookPlyText = std::to_string(bookMaxPly) + "/" + std::to_string(bookMaxMoves);
    sf::Text bookPlyVal(font, bookPlyText, 11);
    bookPlyVal.setFillColor(TEXT_COL);
    auto bpv = bookPlyVal.getLocalBounds();
    bookPlyVal.setPosition({px + SIDE - bpv.size.x - 10, bookPlySliderRect.position.y - 18});
    window.draw(bookPlyVal);

    sf::Text timeLbl(font, "Time", 11);
    timeLbl.setFillColor(DIM_COL);
    timeLbl.setPosition({ix, timeSliderRect.position.y - 18});
    window.draw(timeLbl);
    drawSlider(timeSliderRect, scalarRatio(engineTimeMsLimit, TIME_MIN_MS, TIME_MAX_MS));
    std::string timeVal;
    if (engineTimeMsLimit <= 0) timeVal = "off";
    else if (engineTimeMsLimit % 1000 == 0) timeVal = std::to_string(engineTimeMsLimit / 1000) + "s";
    else timeVal = std::to_string(engineTimeMsLimit) + "ms";
    sf::Text timeTxt(font, timeVal, 11);
    timeTxt.setFillColor(TEXT_COL);
    auto tb = timeTxt.getLocalBounds();
    timeTxt.setPosition({px + SIDE - tb.size.x - 10, timeSliderRect.position.y - 18});
    window.draw(timeTxt);

    sf::Text nodeLbl(font, "Nodes", 11);
    nodeLbl.setFillColor(DIM_COL);
    nodeLbl.setPosition({ix, nodeSliderRect.position.y - 18});
    window.draw(nodeLbl);
    drawSlider(nodeSliderRect, scalarRatio(engineNodeLimit, NODE_MIN, NODE_MAX));
    std::string nodeVal;
    if (engineNodeLimit <= 0) nodeVal = "off";
    else if (engineNodeLimit >= 1000000) nodeVal = std::to_string(engineNodeLimit / 1000000) + "M";
    else nodeVal = std::to_string(engineNodeLimit / 1000) + "k";
    sf::Text nodeTxt(font, nodeVal, 11);
    nodeTxt.setFillColor(TEXT_COL);
    auto nb = nodeTxt.getLocalBounds();
    nodeTxt.setPosition({px + SIDE - nb.size.x - 10, nodeSliderRect.position.y - 18});
    window.draw(nodeTxt);

    drawButton(startFromHereButton, !isLivePosition() && !engineBusy);

    // ---- Separator ----
    float sepY = startFromHereButton.rect.position.y + startFromHereButton.rect.size.y + 12.f;
    sf::RectangleShape sep({(float)(SIDE-20), 1});
    sep.setPosition({px+10, sepY});
    sep.setFillColor({50,50,64});
    window.draw(sep);

    // ---- Move History section ----
    float histHdrY = sepY + 8;
    sf::Text hHdr(font, "MOVE HISTORY", 12);
    hHdr.setStyle(sf::Text::Bold);
    hHdr.setFillColor({160,160,185});
    hHdr.setPosition({ix, histHdrY});
    window.draw(hHdr);

    // Scroll hint
    if ((int)moveHistory.size() > 0) {
        sf::Text scrollHint(font, "scroll: mouse wheel", 10);
        scrollHint.setFillColor({70,70,85});
        auto sb = scrollHint.getLocalBounds();
        scrollHint.setPosition({px + SIDE - sb.size.x - 10, histHdrY + 2});
        window.draw(scrollHint);
    }

    float lineH   = 15.f;
    float histTop = histHdrY + 20;
    // Keep history clear of the capture-balance/info block.
    float histBot = (float)(TOP + SQ * 8 - 34);

    int pairs = ((int)moveHistory.size() + 1) / 2;

    for (int i = historyScroll; i < pairs; i++) {
        float y = histTop + (i - historyScroll) * lineH;
        if (y + lineH > histBot) break;

        int wIdx = i * 2;
        int bIdx = wIdx + 1;
        int lastIdx = currentPly - 1;

        // Move number
        sf::Text num(font, std::to_string(i+1)+".", 12);
        num.setFillColor(DIM_COL);
        num.setPosition({ix, y});
        window.draw(num);

        // White move
        if (wIdx < (int)moveHistory.size()) {
            bool isLast = (wIdx == lastIdx);
            sf::Text wm(font, moveHistory[wIdx], 13);
            wm.setFillColor(isLast ? sf::Color{255,225,80} : TEXT_COL);
            if (isLast) wm.setStyle(sf::Text::Bold);
            wm.setPosition({ix + 26, y});
            window.draw(wm);
        }

        // Black move
        if (bIdx < (int)moveHistory.size()) {
            bool isLast = (bIdx == lastIdx);
            sf::Text bm(font, moveHistory[bIdx], 13);
            bm.setFillColor(isLast ? sf::Color{255,225,80} : sf::Color{185,185,185});
            if (isLast) bm.setStyle(sf::Text::Bold);
            bm.setPosition({ix + 116, y});
            window.draw(bm);
        }
    }

    // ---- Bottom info ----
    float infoY = (float)(TOP + SQ*8 + 8);

    sf::RectangleShape sep2({(float)(SIDE-20), 1});
    sep2.setPosition({px+10, infoY - 2});
    sep2.setFillColor({50,50,64});
    window.draw(sep2);

    std::string playStr = (humanColor == WHITE) ? "White  (side: C, board: F)"
                                                 : "Black  (side: C, board: F)";
    sf::Text playLbl(font, "You:", 12); playLbl.setFillColor(DIM_COL);
    sf::Text playVal(font, playStr, 12); playVal.setFillColor({170,195,255});
    playLbl.setPosition({ix, infoY + 10});
    playVal.setPosition({ix + 34, infoY + 10});
    window.draw(playLbl); window.draw(playVal);

    sf::Text depthLbl(font, "Depth:", 12); depthLbl.setFillColor(DIM_COL);
    std::string depthText = lastEngineMoveFromBook ? "Book" : std::to_string(engine.maxDepthReached);
    sf::Text depthVal(font, depthText, 12);
    depthVal.setFillColor(TEXT_COL);
    depthLbl.setPosition({ix, infoY + 28});
    depthVal.setPosition({ix + 50, infoY + 28});
    window.draw(depthLbl); window.draw(depthVal);

    std::string limStr = "L: ";
    limStr += (engineTimeMsLimit > 0) ? (std::to_string(engineTimeMsLimit) + "ms") : "time off";
    limStr += ", ";
    if (engineNodeLimit > 0) limStr += std::to_string(engineNodeLimit);
    else limStr += "nodes off";
    sf::Text limTxt(font, limStr, 11);
    limTxt.setFillColor(DIM_COL);
    limTxt.setPosition({ix, infoY + 46});
    window.draw(limTxt);

    // Capture balance bar (piece values: P=1, N/B=3, R=5, Q=9)
    float capY = infoY - 26.f;
    sf::Text capHdr(font, "CAPTURE BALANCE", 11);
    capHdr.setFillColor(DIM_COL);
    capHdr.setPosition({ix, capY});
    window.draw(capHdr);

    sf::FloatRect capRect({ix, capY + 14.f}, {(float)(SIDE - 20), 8.f});
    sf::RectangleShape capTrack({capRect.size.x, capRect.size.y});
    capTrack.setPosition(capRect.position);
    capTrack.setFillColor({58,58,74});
    capTrack.setOutlineColor({78,78,96});
    capTrack.setOutlineThickness(1.f);
    window.draw(capTrack);

    sf::RectangleShape capMid({1.f, capRect.size.y + 4.f});
    capMid.setPosition({capRect.position.x + capRect.size.x * 0.5f, capRect.position.y - 2.f});
    capMid.setFillColor({120,120,132});
    window.draw(capMid);

    const int bal = captureBalance();
    // Dynamic scale: normalize by currently capturable maximum material.
    static constexpr int PIECE_VALUES[6] = {1, 3, 3, 5, 9, 0};
    int whiteRemaining = 0;
    int blackRemaining = 0;
    for (int pt = PAWN; pt <= QUEEN; pt++) {
        whiteRemaining += popcount(board.pieces[WHITE][pt]) * PIECE_VALUES[pt];
        blackRemaining += popcount(board.pieces[BLACK][pt]) * PIECE_VALUES[pt];
    }
    int maxCollectible = std::max(1, std::max(whiteRemaining, blackRemaining));
    float t = std::clamp((float)bal / (float)maxCollectible, -1.f, 1.f);
    float halfW = capRect.size.x * 0.5f;
    sf::RectangleShape capFill;
    if (t >= 0.f) {
        capFill.setSize({halfW * t, capRect.size.y});
        capFill.setPosition({capRect.position.x + halfW, capRect.position.y});
        capFill.setFillColor({210, 225, 210});
    } else {
        capFill.setSize({halfW * (-t), capRect.size.y});
        capFill.setPosition({capRect.position.x + halfW + halfW * t, capRect.position.y});
        capFill.setFillColor({220, 185, 185});
    }
    window.draw(capFill);

    std::string capText;
    if (bal > 0) capText = "White +" + std::to_string(bal);
    else if (bal < 0) capText = "Black " + std::to_string(bal);
    else capText = "0";
    sf::Text capVal(font, capText, 11);
    capVal.setFillColor((bal > 0) ? sf::Color{210,225,210}
                                  : (bal < 0) ? sf::Color{220,185,185}
                                              : sf::Color{180,180,180});
    auto cb = capVal.getLocalBounds();
    capVal.setPosition({px + SIDE - cb.size.x - 10, capY});
    window.draw(capVal);
}

void ChessApp::drawPromotionOverlay() {
    sf::RectangleShape shade({(float)BW, (float)(SQ * 8)});
    shade.setPosition({(float)EVAL_W, (float)TOP});
    shade.setFillColor({0, 0, 0, 120});
    window.draw(shade);

    float panelW = 320.f, panelH = 128.f;
    float px = EVAL_W + (BW - panelW) * 0.5f;
    float py = TOP + (SQ * 8 - panelH) * 0.5f;

    sf::RectangleShape panel({panelW, panelH});
    panel.setPosition({px, py});
    panel.setFillColor({38, 38, 52});
    panel.setOutlineColor({90, 110, 150});
    panel.setOutlineThickness(2.f);
    window.draw(panel);

    sf::Text title(font, "Promote pawn (Q / R / B / N)", 16);
    title.setFillColor(TEXT_COL);
    title.setPosition({px + 12.f, py + 8.f});
    window.draw(title);

    const PieceType order[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
    const char labels[4] = {'Q', 'R', 'B', 'N'};
    Color promotedSide = board.sideToMove;
    float by = py + 44.f;
    float bx = px + 12.f;
    float bw = 68.f, bh = 68.f, gap = 8.f;

    for (int i = 0; i < 4; i++) {
        sf::RectangleShape b({bw, bh});
        b.setPosition({bx + i * (bw + gap), by});
        bool hovered = (promotionHoverIndex == i);
        b.setFillColor(hovered ? BTN_HOT : BTN_NORM);
        b.setOutlineColor(hovered ? sf::Color{190,220,255} : sf::Color{88, 98, 126});
        b.setOutlineThickness(hovered ? 2.f : 1.f);
        window.draw(b);

        if (texturesLoaded) {
            sf::Sprite sp(pieceTextures[promotedSide][order[i]]);
            auto sz = pieceTextures[promotedSide][order[i]].getSize();
            float icon = hovered ? (bh - 10.f) : (bh - 14.f);
            sp.setScale({icon / sz.x, icon / sz.y});
            sp.setPosition({bx + i * (bw + gap) + (bw - icon) * 0.5f, by + 6.f});
            window.draw(sp);
        } else {
            sf::Text pc(font, std::string(1, labels[i]), 28);
            pc.setFillColor(TEXT_COL);
            auto pb = pc.getLocalBounds();
            pc.setPosition({bx + i * (bw + gap) + bw/2.f - pb.size.x/2.f - pb.position.x,
                            by + bh/2.f - pb.size.y/2.f - pb.position.y - 6.f});
            window.draw(pc);
        }

        sf::Text key(font, std::string(1, labels[i]), 12);
        key.setFillColor(hovered ? sf::Color{235,245,255} : sf::Color{190, 200, 225});
        key.setPosition({bx + i * (bw + gap) + bw - 14.f, by + bh - 18.f});
        window.draw(key);
    }
}
