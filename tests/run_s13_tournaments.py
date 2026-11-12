"""Run sequential 100-game matches of Voraxis (S13) against Stash v14 and Stash v21."""

import re
import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VORAXIS_EXE = PROJECT_ROOT / "build" / "voraxis.exe"
STASH14_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stash-14.0-windows-x86_64.exe")
STASH21_EXE = Path(r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stash-21.0-windows-x86_64.exe")
CUTECHESS_EXE = Path(r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe")
PGN_DIR = PROJECT_ROOT / "tests" / "pgns"


def run_100_games(opponent_name: str, opponent_exe: Path, pgn_filename: str):
    pgn_path = PGN_DIR / pgn_filename
    pgn_path.parent.mkdir(parents=True, exist_ok=True)
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
        f"cmd={opponent_exe}",
        f"name={opponent_name}",
        "proto=uci",
        "option.Hash=64",
        "-each",
        "tc=5+0.1",
        "-rounds", "100",
        "-concurrency", "4",
        "-recover",
        "-pgnout", str(pgn_path),
    ]

    print("\n" + "=" * 70)
    print(f" STARTING TOURNAMENT: Voraxis vs {opponent_name} (100 Games, 4 Concurrent)")
    print(f" PGN: {pgn_path}")
    print("=" * 70)

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

    print("-" * 70)
    print(f" COMPLETED: Voraxis vs {opponent_name}")
    print(f" Result: +{wins} ={draws} -{losses} ({pts}/{total}, {pct:.2f}%) in {elapsed:.1f}s")
    print("=" * 70)

    return {
        "opponent": opponent_name,
        "wins": wins,
        "draws": draws,
        "losses": losses,
        "points": pts,
        "total": total,
        "pct": pct,
        "duration": elapsed,
        "pgn": str(pgn_path),
    }


def main():
    print("######################################################################")
    print("      VORAXIS S13 BENCHMARK: 100 GAMES VS v14, THEN 100 VS v21       ")
    print("######################################################################")

    res_v14 = run_100_games("Stash-v14.0", STASH14_EXE, "voraxis_s13_vs_stash14_100g.pgn")
    res_v21 = run_100_games("Stash-v21.0", STASH21_EXE, "voraxis_s13_vs_stash21_100g.pgn")

    print("\n" + "#" * 70)
    print("                   FINAL DUAL-TOURNAMENT SUMMARY                      ")
    print("#" * 70)
    print(f"{'Opponent':<15} | {'Score':<12} | {'Win Rate':<10} | {'Record (+W =D -L)':<20} | {'Duration':<10}")
    print("-" * 70)
    for r in [res_v14, res_v21]:
        print(f"{r['opponent']:<15} | {r['points']}/{r['total']:<10} | {r['pct']:<9.2f}% | +{r['wins']} ={r['draws']} -{r['losses']:<14} | {r['duration']:<8.1f}s")
    print("#" * 70)


if __name__ == "__main__":
    main()
