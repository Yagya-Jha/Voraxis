"""Command-line interface for the Voraxis testing and analysis framework."""

import argparse
import sys
from pathlib import Path

from .config import (
    VORAXIS_EXE,
    STOCKFISH_EXE,
    CUTECHESS_EXE,
    DEFAULT_GAMES,
    DEFAULT_PARALLEL,
    DEFAULT_TIME_CONTROL,
)
from .check_env import run_checks
from .match_runner import MatchConfig, run_match
from .results import show_latest_results


def build_parser() -> argparse.ArgumentParser:
    """Build argument parser for CLI commands: check, match, results."""
    parser = argparse.ArgumentParser(
        prog="voraxis-test",
        description="Voraxis Chess Engine Testing & Analysis Framework",
    )

    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # Command: check
    subparsers.add_parser(
        "check",
        help="Validate environment setup, executables, and UCI handshakes.",
    )

    # Command: match
    match_parser = subparsers.add_parser(
        "match",
        help="Run a chess match between Voraxis and Stockfish using Cute Chess CLI.",
    )
    match_parser.add_argument(
        "--games", "-g",
        type=int,
        default=DEFAULT_GAMES,
        help=f"Number of games to play (default: {DEFAULT_GAMES})",
    )
    match_parser.add_argument(
        "--parallel", "-p",
        type=int,
        default=DEFAULT_PARALLEL,
        help=f"Maximum number of concurrent games (default: {DEFAULT_PARALLEL})",
    )
    match_parser.add_argument(
        "--tc", "-t",
        type=str,
        default=DEFAULT_TIME_CONTROL,
        help=f"Time control, e.g. '10+0.1' or 'st=1' (default: '{DEFAULT_TIME_CONTROL}')",
    )
    match_parser.add_argument(
        "--voraxis",
        type=Path,
        default=VORAXIS_EXE,
        help=f"Path to Voraxis executable (default: {VORAXIS_EXE})",
    )
    match_parser.add_argument(
        "--stockfish",
        type=Path,
        default=STOCKFISH_EXE,
        help=f"Path to Stockfish executable (default: {STOCKFISH_EXE})",
    )
    match_parser.add_argument(
        "--cutechess",
        type=Path,
        default=CUTECHESS_EXE,
        help=f"Path to Cute Chess CLI executable (default: {CUTECHESS_EXE})",
    )
    match_parser.add_argument(
        "--pgn-out",
        type=Path,
        default=None,
        help="Custom destination path for output PGN file (default: auto-timestamped)",
    )

    # Command: results
    subparsers.add_parser(
        "results",
        help="Display summary and statistics of the latest completed match.",
    )

    return parser


def main(argv=None) -> int:
    """Main CLI entrypoint."""
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.command:
        parser.print_help()
        return 0

    if args.command == "check":
        return run_checks()

    elif args.command == "match":
        if args.games < 1:
            print(f"[ERROR] --games must be at least 1 (got {args.games})")
            return 1
        if args.parallel < 1:
            print(f"[ERROR] --parallel must be at least 1 (got {args.parallel})")
            return 1

        config = MatchConfig(
            games=args.games,
            parallel=args.parallel,
            time_control=args.tc,
            voraxis_path=args.voraxis,
            stockfish_path=args.stockfish,
            cutechess_path=args.cutechess,
            pgn_path=args.pgn_out,
        )

        try:
            run_match(config)
            return 0
        except Exception as exc:
            print(f"\n[ERROR] Match execution failed: {exc}")
            return 1

    elif args.command == "results":
        return show_latest_results()

    return 0


if __name__ == "__main__":
    sys.exit(main())
