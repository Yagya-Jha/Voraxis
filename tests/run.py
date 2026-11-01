"""Standalone script runner for the Voraxis testing framework."""

import sys
from pathlib import Path

# Ensure project root is in sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tests.cli import main

if __name__ == "__main__":
    sys.exit(main())
