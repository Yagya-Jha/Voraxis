# Voraxis Chess Engine

Voraxis is a high-performance, UCI-compliant chess engine written in C++17. Designed around bitboard architecture, modern search pruning algorithms, fast tapered evaluation, and lockless multi-core scaling, Voraxis achieves an estimated playing strength exceeding 2600 Elo (CCRL Blitz).

---

## Technical Overview

- **Language:** C++17
- **Protocol:** Universal Chess Interface (UCI)
- **Board Representation:** 64-bit Bitboards with Magic Bitboards for sliding piece attacks
- **Search Framework:** Principal Variation Search (PVS / NegaScout) with Iterative Deepening and Aspiration Windows
- **Concurrency:** Native Win32 Lazy SMP thread pool with lockless Transposition Table
- **Endgame Support:** Syzygy 3-4-5 piece tablebases (WDL and DTZ) via embedded Fathom
- **Opening Book:** Polyglot binary opening book format (.bin)

---

## Feature Matrix

### 1. Board Representation and Move Generation
- **Bitboard Architecture:** 64-bit unsigned integer bitboards for individual piece types and composite color/total occupancies.
- **Dual Mailbox Mapping:** 64-element mailbox array maintained synchronously for fast $O(1)$ piece identity lookups.
- **Magic Bitboards:** Precomputed branchless 64-bit Magic Bitboards for rapid $O(1)$ Rook and Bishop attack bitboard generation.
- **Precomputed Attack Tables:** Direct array lookups for King attacks, Knight attacks, and directional pawn attacks.
- **Legal Move Generation:** Complete move generation handling absolute pins, check evasions, double checks, castling path clearance, and en passant legality (including horizontal discovered pin checks).
- **Zobrist Hashing:** 64-bit XOR keys for pieces, castling rights, en passant file, and side to move.
- **Polyglot Compatibility:** Dedicated Polyglot-compliant Zobrist key hashing for opening book integration.
- **Draw Detection:** Automatic detection of the 50-move rule, 3-fold repetition, and insufficient material (KK, KNK, KBK, KNNK).
- **Perft Validation:** Verified against 22 standard Perft benchmark suites (12.58 million nodes verified at >7.9 million nodes per second with 100% accuracy).

---

### 2. Move Ordering and Move Picker
- **Staged Move Generation (MovePicker):** Lazy move generation dividing candidate moves into prioritized phases to avoid generating unneeded quiet moves on early cutoffs:
  1. Transposition Table move (Hash move)
  2. Winning and equal captures (ordered by MVV-LVA and Static Exchange Evaluation)
  3. Queen promotions
  4. Killer moves (2 independent killer move slots per ply)
  5. Countermove heuristic (indexed by previous move's source and destination)
  6. History heuristic combined with 1-ply Continuation History
  7. Bad captures (losing captures demoted by negative SEE score)
  8. Remaining quiet moves
- **Continuation History (conthist):** 4-dimensional move-pair history table `[prevPiece][prevTo][currPiece][currTo]` tracking move efficacy in direct response to opponent replies.
- **Quiet History Malus:** Depth-scaled penalties applied to quiet moves searched prior to a beta cutoff that failed to produce a cutoff.
- **Static Exchange Evaluation (SEE):** Full recursive piece-exchange simulator to evaluate tactical exchange outcomes prior to making a move.

---

### 3. Search Architecture and Pruning
- **Principal Variation Search (PVS):** Full-window search on primary lines with minimal null-window probes on non-PV lines.
- **Iterative Deepening:** Incremental depth progression starting from depth 1 up to maximum depth limits.
- **Aspiration Windows:** Narrow search window bounds (alpha/beta) centered around the previous iteration score with dynamic widening upon fail-high or fail-low.
- **Transposition Table (Lockless XOR Checksum):**
  - Compact 64-bit XOR checksum packing storing score, cached static evaluation, search depth, generation aging tag, and node bound flags (Exact, Lowerbound, Upperbound).
  - Generation aging replacement policy (`ttGen`) ensuring relevant positions from current searches supersede older search trees.
- **Quiescence Search:** Selective capture and promotion search with Stand-Pat evaluation, delta pruning, SEE pruning, and check evasion verification.
- **Reverse Futility Pruning (Static Null Move Pruning):** Evaluates static evaluation margins at shallow depths (depth <= 6) to trigger early beta cutoffs, dynamically tightened by the improving heuristic.
- **Improving Heuristic:** Two-ply static evaluation delta tracking (`staticEvalStack[ply - 2]`) to dynamically tighten or relax search pruning thresholds.
- **Null Move Pruning:** Adaptive depth reduction ($R = 2$ or $R = 3$ for depth >= 6) with verification and zugzwang avoidance.
- **Late Move Pruning (LMP):** Skips quiet moves beyond a depth-scaled threshold at shallow depths (depth <= 4).
- **History Pruning:** Skips quiet moves with poor history scores at shallow depths.
- **Internal Iterative Reductions (IIR):** Automatically reduces target depth by 1 when no TT move is available at depth >= 4.
- **Main-Search SEE Pruning:** Prunes quiet moves and captures that fail static exchange thresholds at shallow plies.
- **Singular Extensions:** Dynamically extends search depth by 1 ply when the primary TT move significantly outscores all alternate candidates.
- **Logarithmic Late Move Reductions (LMR):**
  - Precomputed logarithmic reduction matrix based on `log(depth) * log(moveNumber) / 2.25`.
  - Dynamically modulated by PV node status, improving flag, history score, and continuation history score.

---

### 4. Evaluation Engine
- **PeSTO Tapered Evaluation:** Dual Middlegame (MG) and Endgame (EG) piece-square tables and material values for all six piece types, based on Tomasz Michniewski's PeSTO formulation.
- **Continuous Phase Interpolation:** Smooth 24-step game phase progression interpolating between Middlegame and Endgame scores.
- **Bitboard Pawn Structure Analysis:**
  - Branchless bitboard masks for passed pawns (`passedPawnMask[color][sq]`).
  - Exponential rank-based bonuses for passed pawns.
  - Protected passed pawn bonuses.
  - Penalties for isolated pawns (computed via adjacent file bitboard masks).
  - Penalties for doubled pawns.
- **Piece Bonuses and Mobility:**
  - Safe-square piece mobility evaluated via Magic Bitboards.
  - Bishop pair bonus.
  - Rook on open file, semi-open file, and 7th rank bonuses.
  - Outpost bonuses for Knights supported by pawns.
- **Lightweight King Safety:**
  - Pawn shield integrity checks.
  - Open file and semi-open file proximity penalties.
  - Phase-tapered king danger scaling that naturally fades out toward the endgame.

---

### 5. Multi-Core Scaling (Lazy SMP)
- **Shared Transposition Table:** Single lockless transposition table shared concurrently across all search threads without thread lock contention.
- **Thread Pool Management:** Native Win32 synchronization events (`CreateEvent`, `WaitForSingleObject`) providing microsecond thread waking and parking.
- **Depth Jitter:** Helper threads apply staggered starting depths to diversify search branch exploration.
- **Aggregated Telemetry:** Real-time UCI reporting aggregating total nodes searched and maximum selective depth across all active worker threads.

---

### 6. Endgame Tablebases and Opening Book
- **Syzygy Probing (Fathom Integration):**
  - Root DTZ (Distance-to-Zero) probing for instant, mathematically optimal moves in positions with up to 5 pieces.
  - In-search WDL (Win-Draw-Loss) probing for instant cutoffs at terminal endgame leaves.
- **Polyglot Opening Book:** Support for standard `.bin` opening books with weighted move selection and configurable move horizons.

---

### 7. Universal Chess Interface (UCI) and Time Management
- **Smart Time Allocation:** Dual-boundary time budgeting calculating an optimum target time (`optimumTime`) and an absolute maximum ceiling (`time`) based on clock balance and increment.
- **Move Stability Exit:** Proactive search termination when root best moves remain stable across three or more consecutive iterations at depth >= 6.
- **Configurable UCI Options:**
  - `Hash`: Transposition table size from 1 MB to 2048 MB.
  - `Threads`: Worker search thread count from 1 to 128.
  - `Move Overhead`: Safety buffer (0 to 5000 ms) to prevent network/interface time forfeits.
  - `OwnBook`: Enable or disable the internal opening book.
  - `BookPath`: Filepath to the Polyglot opening book.
  - `SyzygyPath`: Directory path to 3-4-5 piece Syzygy tablebase files.
  - `Clear Hash`: Command to zero out all transposition table entries.

---

## Tournament Results and Rating Progression

Performance tested under strict competitive blitz time controls ($5\text{s} + 0.1\text{s}$ increment, 1 thread, 64 MB Hash) using Cutechess-CLI against calibrated anchor engines:

| Matchup | Games | Score | Win Rate | Elo Performance | Estimated Rating |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Voraxis vs. Stash v14.0** | 100 | 82.0 - 18.0 | 82.0% | +265.4 Elo | ~2319 Elo |
| **Voraxis-S14 vs. Voraxis-S13** | 20 | 14.0 - 6.0 | 70.0% | +147.2 Elo | Regression Verified |
| **Voraxis-S14+ vs. Stash v21.0** | 100 | 36.5 - 63.5 | 36.5% | -96.2 Elo | **~2617 Elo** |

*Note: Stash v21.0 holds an official CCRL Blitz rating of 2713 Elo. Scoring 36.5% against this benchmark establishes Voraxis at ~2617 Elo, decisively surpassing the 2500+ Elo milestone.*

---

## Building from Source

### Prerequisites
- C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- Windows operating system (Win32 thread pool and pipe handling)

### Compilation (GCC / MinGW-w64)
```bash
# Compile with maximum optimization
g++ -O3 -std=c++17 -Isrc/syzygy \
    src/main.cpp src/board.cpp src/movegen.cpp src/magic.cpp \
    src/search.cpp src/threadpool.cpp src/evaluate.cpp src/uci.cpp \
    src/see.cpp src/book.cpp src/syzygy_bridge.cpp src/syzygy/tbprobe.o \
    -o build/voraxis.exe
```

### Running the Perft Verification Suite
```bash
./build/voraxis.exe test
```

---

## Usage with Chess GUIs

Voraxis is compatible with all standard UCI chess graphical user interfaces, including:
- Arena Chess GUI
- Cutechess
- BanksiaGUI
- Fritz / ChessBase

To install, add `voraxis.exe` as a new UCI engine in your GUI of choice. Configure Hash memory and Thread counts to match your hardware profile.
