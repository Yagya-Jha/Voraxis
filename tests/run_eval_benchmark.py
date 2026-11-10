"""20-Game Automated Tournament: Voraxis (Evaluation 4.1-4.6 Complete) vs Voraxis (Pre-Eval Baseline)
Analyzes Elo difference, pawn promotions, and strategic behavior.
"""

import math
import re
import subprocess
import sys
import time
from pathlib import Path
from tests.config import CUTECHESS_EXE, PROJECT_ROOT, PGN_DIR

CMD_NEW = PROJECT_ROOT / "build" / "voraxis.exe"
CMD_OLD = PROJECT_ROOT / "build" / "voraxis_pre_eval.exe"

TOTAL_GAMES = 40
CONCURRENCY = 4
TIME_CONTROL = "5+0.1"


def calculate_elo(score, total):
    if score == total:
        return 800.0, 800.0
    if score == 0:
        return -800.0, -800.0
    win_ratio = score / total
    elo = -400.0 * math.log10((1.0 / win_ratio) - 1.0)
    # 95% Confidence Interval error margin approximation
    std_err = math.sqrt(win_ratio * (1.0 - win_ratio) / total)
    denom = win_ratio * (1.0 - win_ratio) * math.log(10)
    elo_err = 400.0 * std_err / denom if denom > 0 else 0.0
    return elo, elo_err


def analyze_pgn_behavior(pgn_path):
    """Parses PGN to measure pawn promotions and move counts per engine."""
    with open(pgn_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    games = content.split("[Event ")
    stats = {
        "Voraxis_Eval_4_Complete": {"promotions": 0, "wins": 0, "mates_delivered": 0},
        "Voraxis_Pre_Eval": {"promotions": 0, "wins": 0, "mates_delivered": 0},
    }

    for g in games:
        if not g.strip():
            continue
        white_match = re.search(r'\[White\s+"([^"]+)"\]', g)
        black_match = re.search(r'\[Black\s+"([^"]+)"\]', g)
        result_match = re.search(r'\[Result\s+"([^"]+)"\]', g)
        termination_match = re.search(r'\[Termination\s+"([^"]+)"\]', g)

        if not (white_match and black_match and result_match):
            continue

        white = white_match.group(1)
        black = black_match.group(1)
        res = result_match.group(1)
        term = termination_match.group(1) if termination_match else ""

        # Extract movetext (after headers)
        parts = g.split("\n\n", 1)
        if len(parts) < 2:
            continue
        moves = parts[1]

        # Remove comment blocks { ... }
        clean_moves = re.sub(r'\{[^}]*\}', '', moves)
        tokens = clean_moves.split()
        is_white_turn = True
        for tok in tokens:
            if re.match(r'^\d+\.', tok):
                is_white_turn = True
                continue
            if tok in ("1-0", "0-1", "1/2-1/2", "*"):
                continue
            if "=" in tok or (len(tok) == 5 and tok[4] in "qrbnQRBN"):
                eng = white if is_white_turn else black
                if eng in stats:
                    stats[eng]["promotions"] += 1
            is_white_turn = not is_white_turn

        if "mate" in term.lower() or "mate" in moves.lower() or "#" in moves:
            if res == "1-0" and white in stats:
                stats[white]["mates_delivered"] += 1
            elif res == "0-1" and black in stats:
                stats[black]["mates_delivered"] += 1

    return stats


def main():
    total_games = int(sys.argv[1]) if len(sys.argv) > 1 else TOTAL_GAMES
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    pgn_path = PGN_DIR / f"voraxis_eval_4_vs_pre_eval_{total_games}g_{timestamp}.pgn"
    pgn_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(CUTECHESS_EXE),
        "-debug", "all",
        "-engine", f"cmd={CMD_NEW}", f"dir={PROJECT_ROOT}", "name=Voraxis_Eval_4_Complete", "proto=uci",
        "-engine", f"cmd={CMD_OLD}", f"dir={PROJECT_ROOT}", "name=Voraxis_Pre_Eval", "proto=uci",
        "-each", f"tc={TIME_CONTROL}",
        "-rounds", str(total_games),
        "-concurrency", str(CONCURRENCY),
        "-pgnout", str(pgn_path),
    ]

    print("=" * 75)
    print(f"      VORAXIS {total_games}-GAME BENCHMARK: EVALUATION 4 COMPLETE vs PRE-EVAL     ")
    print("=" * 75)
    print(f"Engine 1 (New) : {CMD_NEW.name} (Tapered PeSTO, Pawns, Activity, King Safety, Mop-Up)")
    print(f"Engine 2 (Old) : {CMD_OLD.name} (Search 1.7-1.14 + 3.2 + 8.2 with baseline eval)")
    print(f"Games          : {total_games} (4 simultaneous games)")
    print(f"Time Control   : {TIME_CONTROL}")
    print(f"PGN File       : {pgn_path.name}")
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

    results = {
        "Voraxis_Eval_4_Complete": {"wins": 0, "losses": 0, "draws": 0},
        "Voraxis_Pre_Eval": {"wins": 0, "losses": 0, "draws": 0},
    }

    completed_games = 0

    for line in proc.stdout:
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

            print(f"[{completed_games:02d}/{total_games}] Game {game_num:>2}: {outcome_desc:<45} ({result}) | Reason: {reason}")

    proc.wait()

    w_new = results["Voraxis_Eval_4_Complete"]["wins"]
    l_new = results["Voraxis_Eval_4_Complete"]["losses"]
    d_new = results["Voraxis_Eval_4_Complete"]["draws"]
    pts_new = w_new + 0.5 * d_new

    w_old = results["Voraxis_Pre_Eval"]["wins"]
    l_old = results["Voraxis_Pre_Eval"]["losses"]
    d_old = results["Voraxis_Pre_Eval"]["draws"]
    pts_old = w_old + 0.5 * d_old

    elo_diff, elo_err = calculate_elo(pts_new, total_games)
    behavior = analyze_pgn_behavior(pgn_path)

    print("\n" + "=" * 75)
    print("                          TOURNAMENT FINAL SCORE                          ")
    print("=" * 75)
    print(f"Voraxis_Eval_4_Complete : {pts_new:4.1f} / {total_games}  ({pts_new/total_games*100:5.1f}%)  [{w_new}W - {l_new}L - {d_new}D]")
    print(f"Voraxis_Pre_Eval        : {pts_old:4.1f} / {total_games}  ({pts_old/total_games*100:5.1f}%)  [{w_old}W - {l_old}L - {d_old}D]")
    print("=" * 75)

    print("\n" + "=" * 75)
    print("                        ELO & PERFORMANCE ANALYSIS                        ")
    print("=" * 75)
    sign = "+" if elo_diff >= 0 else ""
    print(f"Estimated Elo Gain      : {sign}{elo_diff:.1f} +/- {elo_err:.1f} Elo (95% CI)")
    print(f"Win Percentage          : {pts_new/total_games*100:.1f}%")
    print("=" * 75)

    print("\n" + "=" * 75)
    print("                        BEHAVIORAL STRATEGY ANALYSIS                      ")
    print("=" * 75)
    print(f"Passed Pawns Promoted to Queen:")
    print(f"  - Voraxis_Eval_4_Complete : {behavior['Voraxis_Eval_4_Complete']['promotions']} promotions")
    print(f"  - Voraxis_Pre_Eval        : {behavior['Voraxis_Pre_Eval']['promotions']} promotions")
    print(f"\nCheckmates Delivered:")
    print(f"  - Voraxis_Eval_4_Complete : {behavior['Voraxis_Eval_4_Complete']['mates_delivered']} mates")
    print(f"  - Voraxis_Pre_Eval        : {behavior['Voraxis_Pre_Eval']['mates_delivered']} mates")
    print("=" * 75)
    print(f"\nPGN file saved: {pgn_path.resolve()}\n")


if __name__ == "__main__":
    main()
