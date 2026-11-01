"""Environment and engine verification script for the Voraxis testing framework."""

import sys
import subprocess
from pathlib import Path
from typing import Tuple

from .config import (
    VORAXIS_EXE,
    STOCKFISH_EXE,
    CUTECHESS_EXE,
    PGN_DIR,
    RESULTS_DIR,
    UCI_TIMEOUT,
)
from .engine import probe_uci_engine, EngineInfo


def check_python() -> Tuple[bool, str]:
    """Verify that Python is available and meets minimum version requirements."""
    version = sys.version_info
    ver_str = f"{version.major}.{version.minor}.{version.micro}"
    if version.major < 3 or (version.major == 3 and version.minor < 8):
        return False, f"Python {ver_str} is older than required 3.8+"
    return True, f"Python {ver_str} ({sys.executable})"


def check_cutechess() -> Tuple[bool, str]:
    """Verify that Cute Chess CLI exists and executes successfully."""
    if not CUTECHESS_EXE.is_file():
        return False, f"Cute Chess CLI executable not found at: {CUTECHESS_EXE}"

    try:
        res = subprocess.run(
            [str(CUTECHESS_EXE), "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5.0,
            check=True,
        )
        first_line = res.stdout.strip().splitlines()[0] if res.stdout else "cutechess-cli"
        return True, f"{first_line} ({CUTECHESS_EXE})"
    except Exception as exc:
        return False, f"Cute Chess CLI failed execution: {exc}"


def check_directory(dir_path: Path, label: str) -> Tuple[bool, str]:
    """Verify that a directory exists and is writable."""
    try:
        dir_path.mkdir(parents=True, exist_ok=True)
        probe_file = dir_path / ".write_probe.tmp"
        probe_file.write_text("ok", encoding="utf-8")
        probe_file.unlink()
        return True, f"Writable {label} directory: {dir_path}"
    except Exception as exc:
        return False, f"{label} directory {dir_path} is not writable: {exc}"


def check_engine(label: str, exe_path: Path) -> Tuple[bool, EngineInfo, str]:
    """Verify engine existence and valid UCI handshake."""
    if not exe_path.is_file():
        return False, EngineInfo(), f"Executable not found at: {exe_path}"

    info = probe_uci_engine(exe_path, timeout=UCI_TIMEOUT)
    if not info.uciok:
        err = info.error or "Failed to receive 'uciok'"
        return False, info, f"{label} UCI check failed: {err}"

    return True, info, f"{label} verified successfully."


def run_checks() -> int:
    """Run all environment and engine checks, printing formatted diagnostics."""
    print("=" * 65)
    print("       VORAXIS TESTING FRAMEWORK - ENVIRONMENT DIAGNOSTICS      ")
    print("=" * 65)

    all_passed = True

    # 1. Python Check
    print("\n[1/5] Checking Python Runtime...")
    py_ok, py_msg = check_python()
    if py_ok:
        print(f"  [OK] {py_msg}")
    else:
        print(f"  [ERROR] {py_msg}")
        all_passed = False

    # 2. Cute Chess CLI Check
    print("\n[2/5] Checking Cute Chess CLI...")
    cc_ok, cc_msg = check_cutechess()
    if cc_ok:
        print(f"  [OK] {cc_msg}")
    else:
        print(f"  [ERROR] {cc_msg}")
        all_passed = False

    # 3. Voraxis UCI Engine Check
    print("\n[3/5] Checking Voraxis Chess Engine...")
    v_ok, v_info, v_msg = check_engine("Voraxis", VORAXIS_EXE)
    if v_ok:
        print(f"  [OK] Path: {VORAXIS_EXE}")
        print(f"  [OK] Name:   {v_info.name}")
        print(f"  [OK] Author: {v_info.author}")
        if v_info.options:
            print(f"  [OK] Options ({len(v_info.options)}):")
            for opt in v_info.options:
                print(f"       - {opt}")
        else:
            print("  [OK] Options: (none registered)")
        print("  [OK] Handshake: 'uciok' received.")
    else:
        print(f"  [ERROR] {v_msg}")
        all_passed = False

    # 4. Stockfish UCI Engine Check
    print("\n[4/5] Checking Stockfish Chess Engine...")
    sf_ok, sf_info, sf_msg = check_engine("Stockfish", STOCKFISH_EXE)
    if sf_ok:
        print(f"  [OK] Path: {STOCKFISH_EXE}")
        print(f"  [OK] Name:   {sf_info.name}")
        print(f"  [OK] Author: {sf_info.author}")
        print(f"  [OK] Options ({len(sf_info.options)} available):")
        for opt in sf_info.options:
            print(f"       - {opt}")
        print("  [OK] Handshake: 'uciok' received.")
    else:
        print(f"  [ERROR] {sf_msg}")
        all_passed = False

    # 5. Output / PGN / Results Directories Check
    print("\n[5/5] Checking Output Directories...")
    pgn_ok, pgn_msg = check_directory(PGN_DIR, "PGN")
    res_ok, res_msg = check_directory(RESULTS_DIR, "Results")
    if pgn_ok:
        print(f"  [OK] {pgn_msg}")
    else:
        print(f"  [ERROR] {pgn_msg}")
        all_passed = False

    if res_ok:
        print(f"  [OK] {res_msg}")
    else:
        print(f"  [ERROR] {res_msg}")
        all_passed = False

    # Summary
    print("\n" + "-" * 65)
    if all_passed:
        print("DIAGNOSTICS RESULT: ALL CHECKS PASSED SUCCESSFULLY.")
        print("-" * 65)
        return 0
    else:
        print("DIAGNOSTICS RESULT: ONE OR MORE CHECKS FAILED.")
        print("-" * 65)
        return 1


if __name__ == "__main__":
    sys.exit(run_checks())
