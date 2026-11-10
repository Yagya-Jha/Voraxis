"""Run automated tournament experiments for Voraxis."""

import re
import subprocess
import sys
import time
from pathlib import Path

from tests.config import (
    VORAXIS_EXE,
    STOCKFISH_EXE,
    CUTECHESS_EXE,
    PGN_DIR,
    RESULTS_DIR,
)

VORAXIS_BASELINE_EXE = VORAXIS_EXE.parent / "voraxis_baseline.exe"


def run_experiment_match(
    name1: str,
    cmd1: Path,
    opts1: dict,
    name2: str,
    cmd2: Path,
    opts2: dict,
    games: int = 5,
    tc: str = "5+0.1",
    concurrency: int = 2,
    pgn_name: str = "match.pgn",
):
    pgn_path = PGN_DIR / pgn_name
    pgn_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(CUTECHESS_EXE),
        "-engine", f"cmd={cmd1}", f"name={name1}", "proto=uci",
    ]
    for k, v in opts1.items():
        cmd.append(f"option.{k}={v}")

    cmd.extend([
        "-engine", f"cmd={cmd2}", f"name={name2}", "proto=uci",
    ])
    for k, v in opts2.items():
        cmd.append(f"option.{k}={v}")

    cmd.extend([
        "-each", f"tc={tc}",
        "-rounds", str(games),
        "-concurrency", str(concurrency),
        "-pgnout", str(pgn_path),
    ])

    print("=" * 65)
    print(f" MATCH: {name1} vs {name2}")
    print(f" Games: {games} | TC: {tc} | PGN: {pgn_path.name}")
    print("=" * 65)

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    finished_re = re.compile(
        r"Finished game (\d+) \(([^)]+)\):\s+([0-9/\*-]+)\s*(?:\{([^}]*)\})?"
    )

    results = {
        name1: {"wins": 0, "losses": 0, "draws": 0},
        name2: {"wins": 0, "losses": 0, "draws": 0},
    }
    game_list = []

    for line in iter(proc.stdout.readline, ""):
        line_s = line.strip()
        m = finished_re.search(line_s)
        if m:
            g_num = int(m.group(1))
            players = m.group(2).split(" vs ")
            w_player = players[0].strip()
            b_player = players[1].strip()
            outcome = m.group(3).strip()
            reason = m.group(4) or "normal"

            if outcome == "1-0":
                winner = w_player
                loser = b_player
            elif outcome == "0-1":
                winner = b_player
                loser = w_player
            else:
                winner = None
                loser = None

            if winner:
                if winner in results:
                    results[winner]["wins"] += 1
                if loser in results:
                    results[loser]["losses"] += 1
                result_str = f"{winner} won ({reason})"
            else:
                results[name1]["draws"] += 1
                results[name2]["draws"] += 1
                result_str = f"Draw ({reason})"

            print(f"  Game {g_num}: [{outcome}] {w_player} (W) vs {b_player} (B) -> {result_str}")
            game_list.append({
                "game": g_num,
                "white": w_player,
                "black": b_player,
                "outcome": outcome,
                "reason": reason,
            })

    proc.wait()

    w1 = results[name1]["wins"]
    l1 = results[name1]["losses"]
    d1 = results[name1]["draws"]
    pts1 = w1 + 0.5 * d1
    pct1 = (pts1 / games) * 100 if games > 0 else 0

    print("-" * 65)
    print(f" RESULT: {name1} {w1}W - {l1}L - {d1}D ({pct1:.1f}%) vs {name2}")
    print("=" * 65 + "\n")

    return {
        "engine1": name1,
        "engine2": name2,
        "wins": w1,
        "losses": l1,
        "draws": d1,
        "points": pts1,
        "pct": pct1,
        "games": game_list,
        "pgn": str(pgn_path),
    }


def main():
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    suite_summary = []

    # 1. Voraxis (New with TT) vs Voraxis (Baseline without TT)
    res0 = run_experiment_match(
        name1="Voraxis_TT",
        cmd1=VORAXIS_EXE,
        opts1={},
        name2="Voraxis_Baseline",
        cmd2=VORAXIS_BASELINE_EXE,
        opts2={},
        games=5,
        tc="5+0.1",
        concurrency=2,
        pgn_name=f"voraxis_tt_vs_baseline_{timestamp}.pgn",
    )
    suite_summary.append(res0)

    # 2. Voraxis vs Stockfish Level 0
    res1 = run_experiment_match(
        name1="Voraxis_TT",
        cmd1=VORAXIS_EXE,
        opts1={},
        name2="Stockfish_L0",
        cmd2=STOCKFISH_EXE,
        opts2={"Skill Level": "0"},
        games=5,
        tc="5+0.1",
        concurrency=2,
        pgn_name=f"voraxis_vs_sf_lvl0_{timestamp}.pgn",
    )
    suite_summary.append(res1)

    # 3. Voraxis vs Stockfish Level 1
    res2 = run_experiment_match(
        name1="Voraxis_TT",
        cmd1=VORAXIS_EXE,
        opts1={},
        name2="Stockfish_L1",
        cmd2=STOCKFISH_EXE,
        opts2={"Skill Level": "1"},
        games=5,
        tc="5+0.1",
        concurrency=2,
        pgn_name=f"voraxis_vs_sf_lvl1_{timestamp}.pgn",
    )
    suite_summary.append(res2)

    # 4. Voraxis vs Stockfish Level 2
    res3 = run_experiment_match(
        name1="Voraxis_TT",
        cmd1=VORAXIS_EXE,
        opts1={},
        name2="Stockfish_L2",
        cmd2=STOCKFISH_EXE,
        opts2={"Skill Level": "2"},
        games=5,
        tc="5+0.1",
        concurrency=2,
        pgn_name=f"voraxis_vs_sf_lvl2_{timestamp}.pgn",
    )
    suite_summary.append(res3)

    print("\n" + "#" * 65)
    print("                      FULL TOURNAMENT REPORT                    ")
    print("#" * 65)
    print(f"{'Opponent':<20} | {'Score (W-L-D)':<15} | {'Points':<8} | {'Win %':<8}")
    print("-" * 65)
    for s in suite_summary:
        opp = s["engine2"]
        score_str = f"{s['wins']}-{s['losses']}-{s['draws']}"
        pts_str = f"{s['points']}/5"
        pct_str = f"{s['pct']:.1f}%"
        print(f"{opp:<20} | {score_str:<15} | {pts_str:<8} | {pct_str:<8}")
    print("#" * 65)


if __name__ == "__main__":
    main()
