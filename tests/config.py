"""Configuration settings and defaults for the Voraxis testing framework."""

import os
from pathlib import Path
from datetime import datetime
from typing import Dict

# Base paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = PROJECT_ROOT / "tests"

# Executable paths
VORAXIS_EXE = Path(
    os.getenv("VORAXIS_PATH", PROJECT_ROOT / "build" / "voraxis.exe")
)

STOCKFISH_EXE = Path(
    os.getenv(
        "STOCKFISH_PATH",
        r"C:\Users\Suraj\Desktop\PiXeLaTED\Yagya Study\CHESS\stockfish\stockfish-windows-x86-64-universal.exe"
    )
)

CUTECHESS_EXE = Path(
    os.getenv(
        "CUTECHESS_PATH",
        r"C:\Users\Suraj\AppData\Local\Programs\Cute Chess\cutechess-cli.exe"
    )
)

# Output directories inside tests
PGN_DIR = Path(
    os.getenv("VORAXIS_PGN_DIR", TESTS_DIR / "pgns")
)

RESULTS_DIR = Path(
    os.getenv("VORAXIS_RESULTS_DIR", TESTS_DIR / "results")
)

LATEST_RESULTS_FILE = RESULTS_DIR / "latest_match.json"

# Timeout settings (in seconds)
UCI_TIMEOUT = float(os.getenv("VORAXIS_UCI_TIMEOUT", "5.0"))

# Match Defaults
DEFAULT_GAMES: int = int(os.getenv("VORAXIS_MATCH_GAMES", "10"))
DEFAULT_PARALLEL: int = int(os.getenv("VORAXIS_MATCH_PARALLEL", "1"))
DEFAULT_TIME_CONTROL: str = os.getenv("VORAXIS_MATCH_TC", "10+0.1")

# Default UCI Options for engines
VORAXIS_OPTIONS: Dict[str, str] = {}
STOCKFISH_OPTIONS: Dict[str, str] = {
    "Threads": "1",
    "Hash": "16",
}


def get_timestamp() -> str:
    """Return formatted current timestamp string."""
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def get_timestamped_pgn_path(prefix: str = "pgn") -> Path:
    """Generate a path for a PGN file formatted as pgn_<timestamp>.pgn inside PGN_DIR."""
    return PGN_DIR / f"{prefix}_{get_timestamp()}.pgn"


def get_timestamped_results_path(prefix: str = "results") -> Path:
    """Generate a path for a results json file formatted as results_<timestamp>.json inside RESULTS_DIR."""
    return RESULTS_DIR / f"{prefix}_{get_timestamp()}.json"
