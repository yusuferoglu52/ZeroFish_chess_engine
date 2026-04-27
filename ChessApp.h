#pragma once
#include <SFML/Graphics.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include "Board.h"
#include "MoveGenerator.h"
#include "Evaluator.h"
#include "Engine.h"

class ChessApp {
public:
    ChessApp();
    ~ChessApp();
    void run();

private:
    // ---- Layout (pixels) ----
    static constexpr int EVAL_W = 24;          // eval bar
    static constexpr int SQ    = 75;           // square size
    static constexpr int TOP   = 52;           // top bar
    static constexpr int SIDE  = 215;          // right panel (depth + history)
    static constexpr int BOT   = 72;           // bottom status bar
    static constexpr int BW    = SQ * 8;       // board width  = 600
    static constexpr int W     = EVAL_W + BW + SIDE; // 839
    static constexpr int H     = TOP + SQ*8 + BOT;   // 724

    sf::RenderWindow window;
    sf::Font         font;
    sf::View         uiView;

    sf::Texture pieceTextures[2][6]; // [color][piece_type]
    bool        texturesLoaded = false;

    Board         board;
    MoveGenerator moveGen;
    Evaluator     evaluator;
    Engine        engine;

    // Interaction
    int               selectedSq = -1;
    std::vector<Move> candidates;
    Color             humanColor = WHITE;
    bool              boardFlipped = false;
    bool              gameOver   = false;
    std::optional<Move> lastMove;          // for yellow highlight
    bool                promotionPending = false;
    std::vector<Move>   promotionMoves;
    int                 promotionHoverIndex = -1;

    // Engine thread
    std::atomic<bool>   engineBusy{false};
    std::mutex          moveMutex;
    std::optional<Move> pendingMove;
    std::thread         engineThread;

    // Eval bar
    int whiteEval = 0;  // centipawns, white's perspective (+= white winning)

    // Move history (LAN strings: "Ng1f3", "e2e4", "O-O", ...)
    std::vector<std::string> moveHistory;
    std::vector<Move>        playedMoves;
    int                      currentPly = 0; // viewed ply in playedMoves
    int historyScroll = 0;

    // Display
    std::string statusMsg;
    int         engineDepth = 5;
    int         engineTimeMsLimit = 2000;
    int         engineNodeLimit = 300000;
    bool        bookMovesEnabled = true;
    bool        lastEngineMoveFromBook = false;
    bool        currentlyOutOfBook = false;
    bool        bookMovesLocked = false;
    int         bookMaxPly = 20; // full moves (0 => unlimited)
    std::string currentOpeningName;
    std::string currentVariationName;

    // Main loop
    void handleEvents();
    void update();
    void render();

    // Input
    void onClick(sf::Vector2i pos);
    void onDepthButton(int depth);
    void onBookToggleButton();
    void onStartFromHereButton();
    void updateBookPlyFromSlider(float x);
    void updateTimeFromSlider(float x);
    void updateNodesFromSlider(float x);

    // Game logic
    void applyMove(const Move& m);
    void startEngine();
    void checkGameOver();
    void updateStatus();
    void updateEval();
    void autoScrollHistory();
    bool isHumanTurn() const;
    bool isLivePosition() const;
    int captureBalance() const;
    void choosePromotion(PieceType pt);
    bool handlePromotionClick(sf::Vector2i pos);
    int  promotionChoiceAt(sf::Vector2f pos) const;
    void updateViewForWindowSize(sf::Vector2u size);
    std::string moveToUci(const Move& m) const;
    std::optional<Move> findBookMove();
    void refreshOpeningLabel();

    // Coordinates
    int          posToSq(sf::Vector2i pos) const;
    sf::Vector2f sqToPos(int s) const;
    sf::Color    squareColor(int s) const;

    // Drawing
    void drawTopBar();
    void drawEvalBar();
    void drawBoard();
    void drawCoordinates();
    void drawHighlights();
    void drawPieces();
    void drawStatusBar();
    void drawSidePanel();
    void drawPromotionOverlay();

    // Depth buttons
    struct Button { sf::FloatRect rect; std::string label; int value; };
    std::vector<Button> depthButtons;
    Button              bookToggleButton;
    Button              startFromHereButton;
    sf::FloatRect       bookPlySliderRect;
    bool                draggingBookPlySlider = false;
    static constexpr int BOOK_PLY_MIN = 0;
    static constexpr int BOOK_PLY_MAX = 40; // fallback max when book cannot be read
    sf::FloatRect       timeSliderRect;
    sf::FloatRect       nodeSliderRect;
    bool                draggingTimeSlider = false;
    bool                draggingNodeSlider = false;
    static constexpr int TIME_MIN_MS = 0;
    static constexpr int TIME_MAX_MS = 20000;
    static constexpr int TIME_STEP_MS = 250;
    static constexpr int NODE_MIN = 0;
    static constexpr int NODE_MAX = 10000000;
    static constexpr int NODE_STEP = 100000;
    void buildDepthButtons();
    void drawButton(const Button& b, bool active);

    // Move notation
    static std::string moveStr(const Move& m);
};
