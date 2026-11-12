"""Run 100-game full-strength match between Voraxis and Stash v14.0.1."""

import re
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VORAXIS_EXE = PROJECT_ROOT / "build" / "voraxis.exe"
STASH14_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stash-14.0-windows-x86_64.exe")
CUTECHESS_EXE = Path(r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe")
PGN_PATH = PROJECT_ROOT / "tests" / "pgns" / "voraxis_s12_vs_stash14_100g.pgn"


def main():
    PGN_PATH.parent.mkdir(parents=True, exist_ok=True)
    if PGN_PATH.exists():
        PGN_PATH.unlink()

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
        f"cmd={STASH14_EXE}",
        "name=Stash-v14.0",
        "proto=uci",
        "option.Hash=64",
        "-each",
        "tc=5+0.1",
        "-rounds", "100",
        "-concurrency", "4",
        "-recover",
        "-pgnout", str(PGN_PATH),
    ]

    print("=" * 65)
    print(" MATCH: Voraxis (Threads=1) vs Stash v14.0.1 (Full Strength)")
    print(" Games: 100 | TC: 5+0.1 | Concurrency: 4")
    print(f" PGN: {PGN_PATH}")
    print("=" * 65)

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
            voraxis_color = "White" if white.lower() == "voraxis" else "Black"

            if result == "1/2-1/2":
                outcome = "DRAW"
                draws += 1
            elif (result == "1-0" and voraxis_color == "White") or (result == "0-1" and voraxis_color == "Black"):
                outcome = "WIN"
                wins += 1
            else:
                outcome = "LOSS"
                losses += 1

            total_done = wins + draws + losses
            pts = wins + 0.5 * draws
            pct = (pts / total_done * 100.0) if total_done else 0.0
            print(f"[{total_done:3d}/100] Game {g_num:3d}: {white} vs {black} -> {result:7s} | Voraxis: {outcome:4s} ({reason}) | Score: +{wins} ={draws} -{losses} ({pct:.1f}%)")
        elif line_s.startswith("Score of") or line_s.startswith("Elo difference") or line_s.startswith("Finished match"):
            print(line_s)

    proc.stdout.close()
    proc.wait()
    elapsed = time.time() - start_time

    total = wins + draws + losses
    pts = wins + 0.5 * draws
    pct = (pts / total * 100.0) if total else 0.0
    print("=" * 65)
    print(f" FINAL RESULT: Voraxis +{wins} ={draws} -{losses} ({pts}/{total}, {pct:.2f}%)")
    print(f" Duration: {elapsed:.1f}s | PGN: {PGN_PATH}")
    print("=" * 65)


if __name__ == "__main__":
    main()
