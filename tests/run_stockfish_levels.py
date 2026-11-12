"""Run automated tournament against Stockfish Skill Levels 3, 4, 5, and 6."""

import os
import re
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VORAXIS_EXE = PROJECT_ROOT / "build" / "voraxis.exe"
STOCKFISH_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stockfish\stockfish-windows-x86-64-universal.exe")
CUTECHESS_EXE = Path(r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe")
PGN_DIR = PROJECT_ROOT / "tests" / "pgns"
RESULTS_DIR = PROJECT_ROOT / "tests" / "results"

PGN_DIR.mkdir(parents=True, exist_ok=True)
RESULTS_DIR.mkdir(parents=True, exist_ok=True)


def run_match_level(level: int, games: int = 5, tc: str = "5+0.1", concurrency: int = 2):
    pgn_path = PGN_DIR / f"voraxis_vs_sf_level_{level}.pgn"
    if pgn_path.exists():
        pgn_path.unlink()

    cmd = [
        str(CUTECHESS_EXE),
        "-engine",
        f"cmd={VORAXIS_EXE}",
        f"dir={PROJECT_ROOT}",
        "name=Voraxis",
        "proto=uci",
        "option.Threads=1",
        "option.Hash=64",
        "option.OwnBook=true",
        "-engine",
        f"cmd={STOCKFISH_EXE}",
        f"name=Stockfish-L{level}",
        "proto=uci",
        f"option.Skill Level={level}",
        "option.Threads=1",
        "option.Hash=16",
        "-each",
        f"tc={tc}",
        "-rounds",
        str(games),
        "-concurrency",
        str(concurrency),
        "-pgnout",
        str(pgn_path),
    ]

    print("\n" + "=" * 65)
    print(f" MATCH: Voraxis vs Stockfish Level {level}")
    print(f" Games: {games} | Time Control: {tc} | Concurrency: {concurrency}")
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

    records = []
    w_wins = 0
    w_draws = 0
    w_losses = 0

    for line in iter(proc.stdout.readline, ""):
        line_s = line.strip()
        m = finished_re.search(line_s)
        if m:
            g_num = int(m.group(1))
            players = m.group(2)
            result = m.group(3)
            reason = m.group(4) or ""

            white, black = [p.strip() for p in players.split("vs")]
            voraxis_color = "White" if white.lower() == "voraxis" else "Black"

            if result == "1/2-1/2":
                outcome = "Draw"
                w_draws += 1
            elif (result == "1-0" and voraxis_color == "White") or (result == "0-1" and voraxis_color == "Black"):
                outcome = "WIN"
                w_wins += 1
            else:
                outcome = "LOSS"
                w_losses += 1

            records.append({
                "game": g_num,
                "white": white,
                "black": black,
                "result": result,
                "reason": reason,
                "voraxis_color": voraxis_color,
                "outcome": outcome,
            })

            print(f"  Game {g_num:2d}: {white} vs {black} -> {result:7s} | Voraxis: {outcome:4s} ({reason})")

    proc.stdout.close()
    proc.wait()

    total = w_wins + w_draws + w_losses
    pts = w_wins + 0.5 * w_draws
    pct = (pts / total * 100) if total > 0 else 0

    print("-" * 65)
    print(f" Result vs Stockfish Level {level}: +{w_wins} ={w_draws} -{w_losses} ({pct:.1f}% | {pts}/{total})")

    return {
        "level": level,
        "games": total,
        "wins": w_wins,
        "draws": w_draws,
        "losses": w_losses,
        "points": pts,
        "percentage": pct,
        "records": records,
        "pgn": str(pgn_path),
    }


def main():
    levels = [3, 4, 5, 6]
    all_results = []

    print("#################################################################")
    print("      VORAXIS vs STOCKFISH BENCHMARK (LEVELS 3, 4, 5, 6)         ")
    print("#################################################################")

    start_time = time.time()
    for lvl in levels:
        res = run_match_level(lvl, games=5, tc="5+0.1", concurrency=3)
        all_results.append(res)

    total_time = time.time() - start_time

    print("\n" + "#" * 65)
    print("                    FINAL TOURNAMENT SUMMARY                     ")
    print("#" * 65)
    print(f"{'Opponent':<20} | {'Score':<10} | {'Percentage':<12} | {'Outcomes (W/D/L)':<16}")
    print("-" * 65)

    tot_w = 0
    tot_d = 0
    tot_l = 0

    for r in all_results:
        lvl_name = f"Stockfish Level {r['level']}"
        score_str = f"{r['points']}/{r['games']}"
        pct_str = f"{r['percentage']:.1f}%"
        wdl_str = f"+{r['wins']} ={r['draws']} -{r['losses']}"
        print(f"{lvl_name:<20} | {score_str:<10} | {pct_str:<12} | {wdl_str:<16}")
        tot_w += r["wins"]
        tot_d += r["draws"]
        tot_l += r["losses"]

    total_games = tot_w + tot_d + tot_l
    total_pts = tot_w + 0.5 * tot_d
    total_pct = (total_pts / total_games * 100) if total_games > 0 else 0

    print("-" * 65)
    print(f"{'OVERALL':<20} | {total_pts}/{total_games}      | {total_pct:.1f}%       | +{tot_w} ={tot_d} -{tot_l}")
    print(f"Total Match Duration: {total_time:.1f} s")
    print("#" * 65)


if __name__ == "__main__":
    main()
