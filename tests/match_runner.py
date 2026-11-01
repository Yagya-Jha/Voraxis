"""Match runner using Cute Chess CLI to manage matches between Voraxis and Stockfish."""

import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .config import (
    VORAXIS_EXE,
    STOCKFISH_EXE,
    CUTECHESS_EXE,
    DEFAULT_GAMES,
    DEFAULT_PARALLEL,
    DEFAULT_TIME_CONTROL,
    VORAXIS_OPTIONS,
    STOCKFISH_OPTIONS,
    LATEST_RESULTS_FILE,
    get_timestamped_pgn_path,
    get_timestamped_results_path,
    get_timestamp,
)


@dataclass
class MatchConfig:
    """Configuration parameters for running a chess match."""
    games: int = DEFAULT_GAMES
    parallel: int = DEFAULT_PARALLEL
    time_control: str = DEFAULT_TIME_CONTROL
    voraxis_path: Path = VORAXIS_EXE
    stockfish_path: Path = STOCKFISH_EXE
    cutechess_path: Path = CUTECHESS_EXE
    pgn_path: Optional[Path] = None
    voraxis_options: Dict[str, str] = field(default_factory=lambda: dict(VORAXIS_OPTIONS))
    stockfish_options: Dict[str, str] = field(default_factory=lambda: dict(STOCKFISH_OPTIONS))


@dataclass
class GameRecord:
    """Individual game outcome record."""
    game_number: int
    white: str
    black: str
    result: str
    reason: str
    voraxis_color: str
    voraxis_outcome: str  # 'win', 'loss', 'draw', or 'failed'


@dataclass
class MatchSummary:
    """Aggregated match statistics and outcome data."""
    timestamp: str
    voraxis_name: str
    stockfish_name: str
    total_games: int
    parallel: int
    time_control: str
    voraxis_wins: int
    voraxis_losses: int
    voraxis_draws: int
    failed_games: int
    score_percentage: float
    voraxis_as_white: Dict[str, int]
    voraxis_as_black: Dict[str, int]
    pgn_file: str
    results_file: str
    games: List[Dict] = field(default_factory=list)


def format_time_control_arg(tc: str) -> str:
    """Format time control string for Cute Chess CLI (-each tc=... or -each st=...)."""
    tc_clean = tc.strip()
    if tc_clean.startswith("tc=") or tc_clean.startswith("st="):
        return tc_clean
    if tc_clean.endswith("s") and tc_clean[:-1].replace(".", "").isdigit():
        return f"st={tc_clean[:-1]}"
    if tc_clean.isdigit():
        return f"st={tc_clean}"
    return f"tc={tc_clean}"


def determine_voraxis_outcome(white: str, black: str, result: str, reason: str) -> Tuple[str, str]:
    """Determine Voraxis's color and outcome ('win', 'loss', 'draw', 'failed')."""
    voraxis_color = "white" if "voraxis" in white.lower() else "black"
    reason_lower = reason.lower()

    # Check for engine failure, disconnection, crash, or illegal move
    is_error = (
        result == "*"
        or "disconnect" in reason_lower
        or "crash" in reason_lower
        or "stall" in reason_lower
        or "illegal" in reason_lower
        or "forfeit" in reason_lower
    )
    if is_error:
        return voraxis_color, "failed"

    if result == "1/2-1/2":
        return voraxis_color, "draw"
    elif result == "1-0":
        return voraxis_color, ("win" if voraxis_color == "white" else "loss")
    elif result == "0-1":
        return voraxis_color, ("win" if voraxis_color == "black" else "loss")

    return voraxis_color, "failed"


def build_cutechess_args(config: MatchConfig, pgn_dest: Path) -> List[str]:
    """Construct argument list for cutechess-cli."""
    args = [str(config.cutechess_path)]

    # Voraxis engine
    args.append("-engine")
    args.append(f"cmd={config.voraxis_path}")
    args.append("name=Voraxis")
    args.append("proto=uci")
    for opt_k, opt_v in config.voraxis_options.items():
        args.append(f"option.{opt_k}={opt_v}")

    # Stockfish engine
    args.append("-engine")
    args.append(f"cmd={config.stockfish_path}")
    args.append("name=Stockfish")
    args.append("proto=uci")
    for opt_k, opt_v in config.stockfish_options.items():
        args.append(f"option.{opt_k}={opt_v}")

    # Match parameters
    tc_arg = format_time_control_arg(config.time_control)
    args.extend(["-each", tc_arg])
    args.extend(["-rounds", str(config.games)])
    args.extend(["-concurrency", str(config.parallel)])
    args.extend(["-pgnout", str(pgn_dest)])

    return args


def run_match(config: MatchConfig) -> MatchSummary:
    """Execute match between Voraxis and Stockfish via Cute Chess CLI, streaming progress."""
    # 1. Path validations
    if not config.voraxis_path.is_file():
        raise FileNotFoundError(f"Voraxis executable not found: {config.voraxis_path}")
    if not config.stockfish_path.is_file():
        raise FileNotFoundError(f"Stockfish executable not found: {config.stockfish_path}")
    if not config.cutechess_path.is_file():
        raise FileNotFoundError(f"Cute Chess CLI executable not found: {config.cutechess_path}")

    # 2. Output destinations
    pgn_path = config.pgn_path or get_timestamped_pgn_path()
    pgn_path.parent.mkdir(parents=True, exist_ok=True)
    results_path = get_timestamped_results_path()
    results_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = build_cutechess_args(config, pgn_path)

    print("=" * 65)
    print("                VORAXIS MATCH RUNNER (CUTE CHESS CLI)            ")
    print("=" * 65)
    print(f"Games to Play : {config.games}")
    print(f"Parallel Games: {config.parallel}")
    print(f"Time Control  : {config.time_control}")
    print(f"Voraxis Engine: {config.voraxis_path.name}")
    print(f"Opponent      : {config.stockfish_path.name}")
    print(f"PGN Target    : {pgn_path}")
    print("-" * 65)
    print("Match in progress...\n")

    # Regular expressions for live output tracking
    finished_re = re.compile(
        r"Finished game (\d+) \(([^)]+)\):\s+([0-9/\*-]+)\s*(?:\{([^}]*)\})?"
    )

    records: List[GameRecord] = []
    w_stats = {"wins": 0, "losses": 0, "draws": 0, "failed": 0}
    b_stats = {"wins": 0, "losses": 0, "draws": 0, "failed": 0}

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    try:
        for line in iter(proc.stdout.readline, ""):
            line_str = line.strip()
            if not line_str:
                continue

            # Stream game start and finish events
            if line_str.startswith("Started game"):
                print(f"  -> {line_str}")
            elif line_str.startswith("Finished game"):
                print(f"  [+] {line_str}")
                m = finished_re.search(line_str)
                if m:
                    g_num = int(m.group(1))
                    players = m.group(2).split(" vs ")
                    white = players[0].strip() if len(players) > 0 else "Unknown"
                    black = players[1].strip() if len(players) > 1 else "Unknown"
                    result = m.group(3).strip()
                    reason = m.group(4).strip() if m.group(4) else ""

                    v_color, v_outcome = determine_voraxis_outcome(white, black, result, reason)
                    record = GameRecord(
                        game_number=g_num,
                        white=white,
                        black=black,
                        result=result,
                        reason=reason,
                        voraxis_color=v_color,
                        voraxis_outcome=v_outcome,
                    )
                    records.append(record)

                    target_stats = w_stats if v_color == "white" else b_stats
                    if v_outcome == "win":
                        target_stats["wins"] += 1
                    elif v_outcome == "loss":
                        target_stats["losses"] += 1
                    elif v_outcome == "draw":
                        target_stats["draws"] += 1
                    else:
                        target_stats["failed"] += 1

            elif line_str.startswith("Finished match"):
                print(f"\n  {line_str}")
            elif line_str.startswith("Warning:") or line_str.startswith("Error:") or "failed" in line_str.lower():
                print(f"  [!] {line_str}")

        proc.wait()
        if proc.returncode != 0 and len(records) == 0:
            raise RuntimeError(f"Cute Chess CLI exited with error code {proc.returncode}")
    except KeyboardInterrupt:
        print("\n[!] Match interrupted by user. Stopping engines...")
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
        raise

    # Calculate overall results
    total_completed = len(records)
    v_wins = w_stats["wins"] + b_stats["wins"]
    v_losses = w_stats["losses"] + b_stats["losses"]
    v_draws = w_stats["draws"] + b_stats["draws"]
    v_failed = w_stats["failed"] + b_stats["failed"]

    # Score percentage = (wins + draws / 2) / total games * 100
    if total_completed > 0:
        score_pct = ((v_wins + (v_draws / 2.0)) / total_completed) * 100.0
    else:
        score_pct = 0.0

    summary = MatchSummary(
        timestamp=get_timestamp(),
        voraxis_name="Voraxis",
        stockfish_name="Stockfish",
        total_games=total_completed,
        parallel=config.parallel,
        time_control=config.time_control,
        voraxis_wins=v_wins,
        voraxis_losses=v_losses,
        voraxis_draws=v_draws,
        failed_games=v_failed,
        score_percentage=score_pct,
        voraxis_as_white=w_stats,
        voraxis_as_black=b_stats,
        pgn_file=str(pgn_path),
        results_file=str(results_path),
        games=[asdict(r) for r in records],
    )

    # Save results to file
    summary_dict = asdict(summary)
    with open(results_path, "w", encoding="utf-8") as f:
        json.dump(summary_dict, f, indent=2)

    with open(LATEST_RESULTS_FILE, "w", encoding="utf-8") as f:
        json.dump(summary_dict, f, indent=2)

    # Print summary report
    print("\n" + "=" * 65)
    print("                          MATCH SUMMARY                          ")
    print("=" * 65)
    print(f"Total Games Played : {total_completed} of {config.games}")
    print(f"Time Control       : {config.time_control}")
    print(f"Concurrency        : {config.parallel}")
    print("-" * 65)
    print(f"Voraxis Wins       : {v_wins}")
    print(f"Voraxis Losses     : {v_losses}")
    print(f"Draws              : {v_draws}")
    if v_failed > 0:
        print(f"Failed / Errors    : {v_failed} (NOT counted as losses)")
    print("-" * 65)
    print(f"Voraxis Score      : {score_pct:.2f}%  ((wins + draws / 2) / {total_completed} * 100)")
    print("-" * 65)
    print(f"As White           : +{w_stats['wins']} -{w_stats['losses']} ={w_stats['draws']}")
    print(f"As Black           : +{b_stats['wins']} -{b_stats['losses']} ={b_stats['draws']}")
    print("-" * 65)
    print(f"PGN Saved To       : {pgn_path}")
    print(f"Results Saved To   : {results_path}")
    print("=" * 65)

    return summary
