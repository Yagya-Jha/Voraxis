"""Package execution entry point for python -m tests."""

import sys
from .cli import main

if __name__ == "__main__":
    sys.exit(main())
