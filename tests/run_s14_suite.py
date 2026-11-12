"""Run Voraxis S14 test suite:
1. 20 games vs Voraxis S13 (measure Section 14 Elo progression)
2. 100 games vs Stash v21.0
"""

import math
import re
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VORAXIS_S14_EXE = PROJECT_ROOT / "build" / "voraxis.exe"
VORAXIS_S13_EXE = PROJECT_ROOT / "build" / "voraxis_s13.exe"
STASH21_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stash-21.0-windows-x86_64.exe")
CUTECHESS_EXE = Path(r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe")
PGN_DIR = PROJECT_ROOT / "tests" / "pgns"


def run_tournament(
    engine1_name: str,
    engine1_exe: Path,
    engine2_name: str,
    engine2_exe: Path,
    rounds: int,
    pgn_filename: str,
    engine1_options: list = None,
    engine2_options: list = None,
):
    pgn_path = PGN_DIR / pgn_filename
    pgn_path.parent.mkdir(parents=True, exist_ok=True)
    if pgn_path.exists():
        pgn_path.unlink()

    cmd = [
        str(CUTECHESS_EXE),
        "-engine",
        f"cmd={engine1_exe}",
        f"dir={PROJECT_ROOT}",
        f"name={engine1_name}",
        "proto=uci",
        "option.Threads=1",
        "option.Hash=64",
    ]
    if engine1_options:
        cmd.extend(engine1_options)

    cmd.extend([
        "-engine",
        f"cmd={engine2_exe}",
        f"name={engine2_name}",
        "proto=uci",
        "option.Threads=1",
        "option.Hash=64",
    ])
    if engine2_options:
        cmd.extend(engine2_options)

    cmd.extend([
        "-each",
        "tc=5+0.1",
        "-rounds", str(rounds),
        "-concurrency", "4",
        "-recover",
        "-pgnout", str(pgn_path),
    ])

    print("\n" + "=" * 75)
    print(f" STARTING: {engine1_name} vs {engine2_name} ({rounds} Games, 4 Concurrency, tc=5+0.1)")
    print(f" PGN: {pgn_path}")
    print("=" * 75)

    start_time = time.time()
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

    wins = 0
    draws = 0
    losses = 0

    for line in iter(proc.stdout.readline, ""):
        line_s = line.strip()
        m = finished_re.search(line_s)
        if m:
            g_num = int(m.group(1))
            players = m.group(2)
            result = m.group(3)
            reason = m.group(4) or ""
            white, black = [p.strip() for p in players.split("vs")]
            e1_color = "White" if white.lower() == engine1_name.lower() else "Black"

            if result == "1/2-1/2":
                outcome = "DRAW"
                draws += 1
            elif (result == "1-0" and e1_color == "White") or (result == "0-1" and e1_color == "Black"):
                outcome = "WIN"
                wins += 1
            else:
                outcome = "LOSS"
                losses += 1

            total_done = wins + draws + losses
            pts = wins + 0.5 * draws
            pct = (pts / total_done * 100.0) if total_done else 0.0
            print(f"[{total_done:3d}/{rounds}] Game {g_num:3d}: {white} vs {black} -> {result:7s} | {engine1_name}: {outcome:4s} ({reason}) | Score: +{wins} ={draws} -{losses} ({pct:.1f}%)")
        elif (line_s.startswith("Score of") or 
              line_s.startswith("Elo difference") or 
              line_s.startswith("Finished match") or
              "playing White" in line_s or
              "playing Black" in line_s):
            print(line_s)

    proc.stdout.close()
    proc.wait()
    elapsed = time.time() - start_time

    total = wins + draws + losses
    pts = wins + 0.5 * draws
    pct = (pts / total * 100.0) if total else 0.0

    elo_diff = 0.0
    if 0.0 < pct < 100.0:
        elo_diff = -400.0 * math.log10(100.0 / pct - 1.0)
    elif pct >= 100.0:
        elo_diff = 800.0
    else:
        elo_diff = -800.0

    print("-" * 75)
    print(f" COMPLETED: {engine1_name} vs {engine2_name}")
    print(f" Result: +{wins} ={draws} -{losses} ({pts}/{total}, {pct:.2f}%) | Elo Diff: {elo_diff:+.1f} in {elapsed:.1f}s")
    print("=" * 75)

    return {
        "engine1": engine1_name,
        "engine2": engine2_name,
        "wins": wins,
        "draws": draws,
        "losses": losses,
        "points": pts,
        "total": total,
        "pct": pct,
        "elo": elo_diff,
        "duration": elapsed,
        "pgn": str(pgn_path),
    }


def main():
    print("###########################################################################")
    print("      VORAXIS S14 BENCHMARK SUITE: 20G VS S13, THEN 100G VS STASH v21     ")
    print("###########################################################################")

    # Match 1: Voraxis-S14 vs Voraxis-S13 (20 games)
    res_s13 = run_tournament(
        engine1_name="Voraxis-S14",
        engine1_exe=VORAXIS_S14_EXE,
        engine2_name="Voraxis-S13",
        engine2_exe=VORAXIS_S13_EXE,
        rounds=20,
        pgn_filename="s14_vs_s13_20g.pgn",
    )

    # Match 2: Voraxis-S14 vs Stash v21.0 (100 games)
    res_stash21 = run_tournament(
        engine1_name="Voraxis-S14",
        engine1_exe=VORAXIS_S14_EXE,
        engine2_name="Stash-v21.0",
        engine2_exe=STASH21_EXE,
        rounds=100,
        pgn_filename="voraxis_s14_vs_stash21_100g.pgn",
        engine1_options=["option.OwnBook=true"],
    )

    print("\n" + "#" * 75)
    print("                     OVERALL BENCHMARK SUMMARY                             ")
    print("#" * 75)
    print(f"{'Matchup':<25} | {'Score':<10} | {'Win %':<8} | {'Record':<16} | {'Elo Diff':<10} | {'Time':<8}")
    print("-" * 75)
    for r in [res_s13, res_stash21]:
        matchup = f"{r['engine1']} vs {r['engine2']}"
        score_str = f"{r['points']}/{r['total']}"
        rec_str = f"+{r['wins']} ={r['draws']} -{r['losses']}"
        print(f"{matchup:<25} | {score_str:<10} | {r['pct']:<7.1f}% | {rec_str:<16} | {r['elo']:<+9.1f} | {r['duration']:<6.1f}s")
    print("#" * 75)


if __name__ == "__main__":
    main()
