"""100-game match between Voraxis (+1 ply depth) and Stash v21.0."""

import math
import re
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VORAXIS_EXE = PROJECT_ROOT / "build" / "voraxis.exe"
STASH21_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stash-21.0-windows-x86_64.exe")
CUTECHESS_EXE = Path(r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe")
PGN_PATH = PROJECT_ROOT / "tests" / "pgns" / "voraxis_s14_plus1ply_vs_stash21_100g.pgn"


def main():
    PGN_PATH.parent.mkdir(parents=True, exist_ok=True)
    if PGN_PATH.exists():
        PGN_PATH.unlink()

    cmd = [
        str(CUTECHESS_EXE),
        "-engine",
        f"cmd={VORAXIS_EXE}",
        f"dir={PROJECT_ROOT}",
        "name=Voraxis-S14+",
        "proto=uci",
        "option.Threads=1",
        "option.Hash=64",
        "option.OwnBook=true",
        "-engine",
        f"cmd={STASH21_EXE}",
        "name=Stash-v21.0",
        "proto=uci",
        "option.Threads=1",
        "option.Hash=64",
        "-each",
        "tc=5+0.1",
        "-rounds", "100",
        "-concurrency", "4",
        "-recover",
        "-pgnout", str(PGN_PATH),
    ]

    print("\n" + "=" * 75)
    print(" STARTING: Voraxis-S14+ (+1 ply depth) vs Stash-v21.0 (100 Games, 4 Concurrency, tc=5+0.1)")
    print(f" PGN: {PGN_PATH}")
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
            e1_color = "White" if white.lower().startswith("voraxis") else "Black"

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
            print(f"[{total_done:3d}/100] Game {g_num:3d}: {white} vs {black} -> {result:7s} | Voraxis: {outcome:4s} ({reason}) | Score: +{wins} ={draws} -{losses} ({pct:.1f}%)")
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
    print(" COMPLETED: Voraxis-S14+ vs Stash-v21.0")
    print(f" Result: +{wins} ={draws} -{losses} ({pts}/{total}, {pct:.2f}%) | Elo Diff: {elo_diff:+.1f} in {elapsed:.1f}s")
    print("=" * 75)


if __name__ == "__main__":
    main()
