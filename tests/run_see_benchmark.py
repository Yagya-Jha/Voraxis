"""20-Game Automated Tournament & NPS Profiler: Voraxis_SEE vs Voraxis_No_SEE"""

import re
import subprocess
import time
from pathlib import Path
from tests.config import CUTECHESS_EXE, PROJECT_ROOT, PGN_DIR

CMD_SEE = PROJECT_ROOT / "build" / "voraxis.exe"
CMD_NO_SEE = PROJECT_ROOT / "build" / "voraxis_no_see.exe"

TOTAL_GAMES = 20
CONCURRENCY = 4
TIME_CONTROL = "5+0.1"


def main():
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    pgn_path = PGN_DIR / f"voraxis_see_vs_no_see_20g_{timestamp}.pgn"
    pgn_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(CUTECHESS_EXE),
        "-debug", "all",
        "-engine", f"cmd={CMD_SEE}", f"dir={PROJECT_ROOT}", "name=Voraxis_SEE", "proto=uci",
        "-engine", f"cmd={CMD_NO_SEE}", f"dir={PROJECT_ROOT}", "name=Voraxis_No_SEE", "proto=uci",
        "-each", f"tc={TIME_CONTROL}",
        "-rounds", str(TOTAL_GAMES),
        "-concurrency", str(CONCURRENCY),
        "-pgnout", str(pgn_path),
    ]

    print("=" * 70)
    print("      VORAXIS 20-GAME BENCHMARK: SEE vs NON-SEE (CONCURRENCY: 4)     ")
    print("=" * 70)
    print(f"Engine 1 (SEE)    : {CMD_SEE.name}")
    print(f"Engine 2 (No-SEE) : {CMD_NO_SEE.name}")
    print(f"Games             : {TOTAL_GAMES} (4 simultaneous games)")
    print(f"Time Control      : {TIME_CONTROL}")
    print(f"PGN File          : {pgn_path.name}")
    print("=" * 70 + "\n")

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
    # Matches: <Voraxis_SEE(0): info depth 6 score cp 25 nodes 31924 time 41 pv ...
    info_re = re.compile(
        r"<\s*([A-Za-z0-9_]+)\(\d+\)\s*:\s*info depth (\d+) score cp (-?\d+) nodes (\d+) time (\d+)"
    )

    results = {
        "Voraxis_SEE": {"wins": 0, "losses": 0, "draws": 0},
        "Voraxis_No_SEE": {"wins": 0, "losses": 0, "draws": 0},
    }

    # Tracking per-move search stats: list of (nodes, time_ms)
    search_data = {
        "Voraxis_SEE": {"nodes": [], "time_ms": [], "nps_samples": []},
        "Voraxis_No_SEE": {"nodes": [], "time_ms": [], "nps_samples": []},
    }

    # Track currently pending search iteration per engine instance
    # map: engine_name -> (last_nodes, last_time_ms)
    last_info = {}

    start_time = time.time()

    for line in iter(proc.stdout.readline, ""):
        line_s = line.strip()

        # Parse engine info lines for nodes and time
        im = info_re.search(line_s)
        if im:
            eng, depth, score, nodes_s, ms_s = im.groups()
            nodes = int(nodes_s)
            ms = int(ms_s)
            if eng in search_data and ms > 0 and nodes > 0:
                # Record sample
                nps = (nodes * 1000) / ms
                search_data[eng]["nodes"].append(nodes)
                search_data[eng]["time_ms"].append(ms)
                search_data[eng]["nps_samples"].append(nps)

        # Parse game finished
        fm = finished_re.search(line_s)
        if fm:
            g_num = int(fm.group(1))
            players = fm.group(2).split(" vs ")
            w_player = players[0].strip()
            b_player = players[1].strip()
            outcome = fm.group(3).strip()
            reason = fm.group(4) or "normal"

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
                results[winner]["wins"] += 1
                results[loser]["losses"] += 1
                res_str = f"{winner} won ({reason})"
            else:
                results["Voraxis_SEE"]["draws"] += 1
                results["Voraxis_No_SEE"]["draws"] += 1
                res_str = f"Draw ({reason})"

            w_tot = results["Voraxis_SEE"]["wins"]
            l_tot = results["Voraxis_SEE"]["losses"]
            d_tot = results["Voraxis_SEE"]["draws"]

            # Quick running NPS calculation
            samples_see = search_data["Voraxis_SEE"]["nps_samples"]
            samples_nosee = search_data["Voraxis_No_SEE"]["nps_samples"]
            cur_nps_see = sum(samples_see) / len(samples_see) if samples_see else 0
            cur_nps_nosee = sum(samples_nosee) / len(samples_nosee) if samples_nosee else 0

            print(f"[{g_num:>2}/{TOTAL_GAMES}] {w_player:<14} (W) vs {b_player:<14} (B) -> {res_str}")
            print(f"       Score: SEE {w_tot}W - {l_tot}L - {d_tot}D | NPS: SEE {cur_nps_see:,.0f} vs No-SEE {cur_nps_nosee:,.0f}")

    proc.wait()
    total_elapsed = time.time() - start_time

    # Final report
    w = results["Voraxis_SEE"]["wins"]
    l = results["Voraxis_SEE"]["losses"]
    d = results["Voraxis_SEE"]["draws"]
    pts = w + 0.5 * d
    pct = (pts / TOTAL_GAMES) * 100

    see_nodes_total = sum(search_data["Voraxis_SEE"]["nodes"])
    see_time_total = sum(search_data["Voraxis_SEE"]["time_ms"]) / 1000.0
    nosee_nodes_total = sum(search_data["Voraxis_No_SEE"]["nodes"])
    nosee_time_total = sum(search_data["Voraxis_No_SEE"]["time_ms"]) / 1000.0

    avg_nps_see = (see_nodes_total / see_time_total) if see_time_total > 0 else 0
    avg_nps_nosee = (nosee_nodes_total / nosee_time_total) if nosee_time_total > 0 else 0

    diff_nps = avg_nps_see - avg_nps_nosee
    pct_diff = ((avg_nps_see - avg_nps_nosee) / avg_nps_nosee * 100.0) if avg_nps_nosee > 0 else 0

    print("\n" + "=" * 70)
    print("                    FINAL TOURNAMENT & NPS REPORT                     ")
    print("=" * 70)
    print(f"Total Matches Played : {TOTAL_GAMES}")
    print(f"Concurrency Level    : {CONCURRENCY} parallel threads")
    print(f"Elapsed Time         : {total_elapsed:.1f} s")
    print("-" * 70)
    print(f"Voraxis_SEE Score    : {w} Wins, {l} Losses, {d} Draws")
    print(f"Points Scored        : {pts} / {TOTAL_GAMES} ({pct:.1f}%)")
    print("-" * 70)
    print(f"{'Metric':<25} | {'Voraxis_SEE':<18} | {'Voraxis_No_SEE':<18}")
    print("-" * 70)
    print(f"{'Total Nodes Sampled':<25} | {see_nodes_total:<18,d} | {nosee_nodes_total:<18,d}")
    print(f"{'Total Time Sampled':<25} | {see_time_total:<18.2f} s | {nosee_time_total:<18.2f} s")
    print(f"{'Average NPS':<25} | {avg_nps_see:<18,.0f} | {avg_nps_nosee:<18,.0f}")
    print("-" * 70)
    sign = "+" if diff_nps >= 0 else ""
    print(f"NPS Difference       : {sign}{diff_nps:,.0f} nodes/sec ({sign}{pct_diff:.2f}%)")
    print("=" * 70)


if __name__ == "__main__":
    main()
