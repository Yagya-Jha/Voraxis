"""UCI Engine interaction and probing module."""

import subprocess
import threading
import queue
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional


@dataclass
class EngineInfo:
    """Holds parsed UCI identification and configuration details."""
    name: str = "Unknown"
    author: str = "Unknown"
    options: List[str] = field(default_factory=list)
    uciok: bool = False
    raw_lines: List[str] = field(default_factory=list)
    error: Optional[str] = None


class UCIEngineProcess:
    """Manages an interactive UCI chess engine subprocess."""

    def __init__(self, executable_path: Path):
        self.path = Path(executable_path)
        self.proc: Optional[subprocess.Popen] = None
        self._queue: queue.Queue = queue.Queue()
        self._thread: Optional[threading.Thread] = None

    def start(self):
        """Starts the engine subprocess and background line reader thread."""
        self.proc = subprocess.Popen(
            [str(self.path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        def _reader():
            if not self.proc or not self.proc.stdout:
                return
            try:
                for line in iter(self.proc.stdout.readline, ""):
                    if line:
                        self._queue.put(line.strip())
            except Exception:
                pass

        self._thread = threading.Thread(target=_reader, daemon=True)
        self._thread.start()

    def send(self, command: str):
        """Send a command string to the engine's standard input."""
        if self.proc and self.proc.stdin:
            self.proc.stdin.write(command.strip() + "\n")
            self.proc.stdin.flush()

    def read_line(self, timeout: float = 1.0) -> Optional[str]:
        """Read a line from the engine output queue with timeout."""
        try:
            return self._queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def quit(self):
        """Send quit command and gracefully close the engine process."""
        if self.proc:
            try:
                self.send("quit")
                self.proc.wait(timeout=2.0)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass
            finally:
                self.proc = None


def probe_uci_engine(executable_path: Path, timeout: float = 5.0) -> EngineInfo:
    """Launch engine, send 'uci', collect details until 'uciok', and close cleanly."""
    path = Path(executable_path)
    if not path.is_file():
        return EngineInfo(error=f"Executable not found at: {path}")

    engine = UCIEngineProcess(path)
    info = EngineInfo()

    try:
        engine.start()
        engine.send("uci")

        deadline = time.time() + timeout
        while time.time() < deadline:
            remaining = max(0.1, deadline - time.time())
            line = engine.read_line(timeout=remaining)
            if line is None:
                break

            info.raw_lines.append(line)

            if line.startswith("id name "):
                info.name = line[len("id name "):].strip()
            elif line.startswith("id author "):
                info.author = line[len("id author "):].strip()
            elif line.startswith("option name "):
                info.options.append(line)
            elif line == "uciok":
                info.uciok = True
                break

        if not info.uciok:
            info.error = f"Timed out after {timeout:.1f}s waiting for 'uciok' response."

    except Exception as exc:
        info.error = f"Failed to probe engine: {exc}"
    finally:
        engine.quit()

    return info
