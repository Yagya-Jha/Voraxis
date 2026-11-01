"""Results viewer for Voraxis matches."""

import json
import sys
from pathlib import Path
from .config import LATEST_RESULTS_FILE


def show_latest_results() -> int:
    """Display the summary of the latest completed match."""
    if not LATEST_RESULTS_FILE.is_file():
        print(f"No completed match results found.")
        print(f"Run a match first using: python -m tests match")
        return 1

    try:
        with open(LATEST_RESULTS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception as exc:
        print(f"Error reading latest results from {LATEST_RESULTS_FILE}: {exc}")
        return 1

    print("=" * 65)
    print("                     LATEST MATCH SUMMARY                        ")
    print("=" * 65)
    print(f"Match Timestamp    : {data.get('timestamp', 'Unknown')}")
    print(f"Engines            : {data.get('voraxis_name', 'Voraxis')} vs {data.get('stockfish_name', 'Stockfish')}")
    print(f"Total Games Played : {data.get('total_games', 0)}")
    print(f"Concurrency        : {data.get('parallel', 1)}")
    print(f"Time Control       : {data.get('time_control', 'N/A')}")
    print("-" * 65)

    v_wins = data.get("voraxis_wins", 0)
    v_losses = data.get("voraxis_losses", 0)
    v_draws = data.get("voraxis_draws", 0)
    v_failed = data.get("failed_games", 0)
    score_pct = data.get("score_percentage", 0.0)

    print(f"Voraxis Wins       : {v_wins}")
    print(f"Voraxis Losses     : {v_losses}")
    print(f"Draws              : {v_draws}")
    if v_failed > 0:
        print(f"Failed / Errors    : {v_failed} (NOT counted as losses)")
    print("-" * 65)
    print(f"Voraxis Score      : {score_pct:.2f}%")

    w_stats = data.get("voraxis_as_white", {})
    b_stats = data.get("voraxis_as_black", {})
    if w_stats:
        print(f"As White           : +{w_stats.get('wins', 0)} -{w_stats.get('losses', 0)} ={w_stats.get('draws', 0)}")
    if b_stats:
        print(f"As Black           : +{b_stats.get('wins', 0)} -{b_stats.get('losses', 0)} ={b_stats.get('draws', 0)}")

    print("-" * 65)
    print(f"PGN File           : {data.get('pgn_file', 'None')}")
    print(f"Results File       : {data.get('results_file', 'None')}")

    games = data.get("games", [])
    if games:
        print("-" * 65)
        print("Game History:")
        for g in games[:20]:
            num = g.get("game_number", 0)
            white = g.get("white", "W")
            black = g.get("black", "B")
            res = g.get("result", "*")
            reason = g.get("reason", "")
            outcome = g.get("voraxis_outcome", "unknown").upper()
            reason_str = f" ({reason})" if reason else ""
            print(f"  Game {num:2d}: [{outcome:6s}] {white} vs {black} : {res}{reason_str}")
        if len(games) > 20:
            print(f"  ... and {len(games) - 20} more games in PGN file.")
    print("=" * 65)

    return 0


if __name__ == "__main__":
    sys.exit(show_latest_results())
