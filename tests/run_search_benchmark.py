"""10-Game Tournament: Voraxis_Enhanced (Search 1.7-1.14) vs Voraxis_SEE_Baseline"""

import re
import subprocess
import time
from pathlib import Path
from tests.config import CUTECHESS_EXE, PROJECT_ROOT, PGN_DIR

CMD_ENHANCED = PROJECT_ROOT / "build" / "voraxis.exe"
CMD_BASELINE = PROJECT_ROOT / "build" / "voraxis_with_see.exe"

TOTAL_GAMES = 10
CONCURRENCY = 4
TIME_CONTROL = "5+0.1"


def main():
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    pgn_path = PGN_DIR / f"voraxis_enhanced_vs_baseline_10g_{timestamp}.pgn"
    pgn_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(CUTECHESS_EXE),
        "-debug", "all",
        "-engine", f"cmd={CMD_ENHANCED}", f"dir={PROJECT_ROOT}", "name=Voraxis_Enhanced", "proto=uci",
        "-engine", f"cmd={CMD_BASELINE}", f"dir={PROJECT_ROOT}", "name=Voraxis_Baseline_SEE", "proto=uci",
        "-each", f"tc={TIME_CONTROL}",
        "-rounds", str(TOTAL_GAMES),
        "-concurrency", str(CONCURRENCY),
        "-pgnout", str(pgn_path),
    ]

    print("=" * 75)
    print("      VORAXIS 10-GAME BENCHMARK: ENHANCED (1.7-1.14) vs BASELINE (SEE)     ")
    print("=" * 75)
    print(f"Engine 1 (Enhanced) : {CMD_ENHANCED.name} (LMR, Razoring, Singular/Check Ext, etc.)")
    print(f"Engine 2 (Baseline) : {CMD_BASELINE.name} (Books + Syzygy + SEE)")
    print(f"Games               : {TOTAL_GAMES} (4 simultaneous games)")
    print(f"Time Control        : {TIME_CONTROL}")
    print(f"PGN File            : {pgn_path.name}")
    print("=" * 75 + "\n")

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
    info_re = re.compile(
        r"<\s*([A-Za-z0-9_]+)\(\d+\)\s*:\s*info depth (\d+) score (?:cp|mate) (-?\d+) nodes (\d+) time (\d+)"
    )

    results = {
        "Voraxis_Enhanced": {"wins": 0, "losses": 0, "draws": 0},
        "Voraxis_Baseline_SEE": {"wins": 0, "losses": 0, "draws": 0},
    }

    nps_stats = {
        "Voraxis_Enhanced": {"total_nodes": 0, "total_time_ms": 0, "sample_count": 0, "max_depth": 0},
        "Voraxis_Baseline_SEE": {"total_nodes": 0, "total_time_ms": 0, "sample_count": 0, "max_depth": 0},
    }

    completed_games = 0

    for line in proc.stdout:
        info_match = info_re.search(line)
        if info_match:
            eng_name, depth_str, score_str, nodes_str, time_str = info_match.groups()
            if eng_name in nps_stats:
                d = int(depth_str)
                n = int(nodes_str)
                t = int(time_str)
                if t >= 10:
                    nps_stats[eng_name]["total_nodes"] += n
                    nps_stats[eng_name]["total_time_ms"] += t
                    nps_stats[eng_name]["sample_count"] += 1
                if d > nps_stats[eng_name]["max_depth"]:
                    nps_stats[eng_name]["max_depth"] = d

        m = finished_re.search(line)
        if m:
            completed_games += 1
            game_num = m.group(1)
            players = m.group(2)
            result = m.group(3)
            reason = m.group(4) or ""

            white, black = [p.strip() for p in players.split("vs")]
            if result == "1-0":
                winner, loser = white, black
                results[winner]["wins"] += 1
                results[loser]["losses"] += 1
                outcome_desc = f"{winner} WON as White"
            elif result == "0-1":
                winner, loser = black, white
                results[winner]["wins"] += 1
                results[loser]["losses"] += 1
                outcome_desc = f"{winner} WON as Black"
            else:
                results[white]["draws"] += 1
                results[black]["draws"] += 1
                outcome_desc = f"DRAW between {white} and {black}"

            print(f"[{completed_games:02d}/{TOTAL_GAMES}] Game {game_num:>2}: {outcome_desc:<36} ({result}) | Reason: {reason}")

    proc.wait()

    w_enh = results["Voraxis_Enhanced"]["wins"]
    l_enh = results["Voraxis_Enhanced"]["losses"]
    d_enh = results["Voraxis_Enhanced"]["draws"]
    pts_enh = w_enh + 0.5 * d_enh

    w_base = results["Voraxis_Baseline_SEE"]["wins"]
    l_base = results["Voraxis_Baseline_SEE"]["losses"]
    d_base = results["Voraxis_Baseline_SEE"]["draws"]
    pts_base = w_base + 0.5 * d_base

    print("\n" + "=" * 75)
    print("                          TOURNAMENT FINAL SCORE                          ")
    print("=" * 75)
    print(f"Voraxis_Enhanced     : {pts_enh:4.1f} / {TOTAL_GAMES}  ({pts_enh/TOTAL_GAMES*100:5.1f}%)  [{w_enh}W - {l_enh}L - {d_enh}D]")
    print(f"Voraxis_Baseline_SEE : {pts_base:4.1f} / {TOTAL_GAMES}  ({pts_base/TOTAL_GAMES*100:5.1f}%)  [{w_base}W - {l_base}L - {d_base}D]")
    print("=" * 75)

    print("\n" + "=" * 75)
    print("                        SEARCH PERFORMANCE ANALYSIS                       ")
    print("=" * 75)
    for name, stats in nps_stats.items():
        tot_n = stats["total_nodes"]
        tot_t = stats["total_time_ms"]
        avg_nps = (tot_n * 1000 / tot_t) if tot_t > 0 else 0
        print(f"[{name}]")
        print(f"  - Peak Search Depth : {stats['max_depth']}")
        print(f"  - Total Tracked Nodes : {tot_n:,}")
        print(f"  - Search Time Tracked : {tot_t / 1000:.2f} s")
        print(f"  - Effective Avg NPS   : {avg_nps:,.0f} nodes/sec")
    print("=" * 75)
    print(f"\nPGN file saved: {pgn_path.resolve()}\n")


if __name__ == "__main__":
    main()
