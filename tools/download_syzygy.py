"""Automated, verified, multi-threaded downloader for 3-4-5 piece Syzygy tablebases."""

import hashlib
import os
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

BASE_URL = "http://tablebase.sesse.net/syzygy/3-4-5/"
TARGET_DIR = Path(__file__).resolve().parent.parent / "books" / "syzygy"

BROWSER_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36",
    "Accept": "*/*",
}


def compute_md5(file_path: Path) -> str:
    """Compute MD5 checksum of a file."""
    hasher = hashlib.md5()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest().lower()


def fetch_checksums() -> dict:
    """Fetch and parse official checksum.md5 from server."""
    checksum_url = BASE_URL + "checksum.md5"
    print(f"Fetching official checksums from {checksum_url}...")
    req = urllib.request.Request(checksum_url, headers=BROWSER_HEADERS)
    with urllib.request.urlopen(req, timeout=15) as resp:
        content = resp.read().decode("utf-8")

    checksums = {}
    for line in content.strip().splitlines():
        parts = line.split()
        if len(parts) >= 2:
            md5 = parts[0].strip().lower()
            fname = parts[1].strip().lstrip("*")
            checksums[fname] = md5
    return checksums


def download_single_file(fname: str, expected_md5: str, target_dir: Path) -> tuple:
    """Download a single tablebase file and verify its MD5 checksum."""
    dest_path = target_dir / fname
    temp_path = target_dir / (fname + ".part")

    # Check if already downloaded and verified
    if dest_path.is_file():
        actual_md5 = compute_md5(dest_path)
        if actual_md5 == expected_md5:
            return fname, dest_path.stat().st_size, "skipped (already verified)"

    file_url = BASE_URL + fname
    max_retries = 5

    for attempt in range(max_retries):
        try:
            req = urllib.request.Request(file_url, headers=BROWSER_HEADERS)
            with urllib.request.urlopen(req, timeout=30) as resp:
                total_bytes = 0
                with open(temp_path, "wb") as out:
                    while chunk := resp.read(65536):
                        out.write(chunk)
                        total_bytes += len(chunk)

            # Verify checksum
            actual_md5 = compute_md5(temp_path)
            if actual_md5 != expected_md5:
                if temp_path.exists():
                    temp_path.unlink()
                raise ValueError(f"MD5 mismatch: expected {expected_md5}, got {actual_md5}")

            # Atomically rename .part to final file
            if dest_path.exists():
                dest_path.unlink()
            temp_path.rename(dest_path)
            return fname, total_bytes, "downloaded & verified"

        except Exception as exc:
            if temp_path.exists():
                temp_path.unlink()
            if attempt == max_retries - 1:
                return fname, 0, f"failed: {exc}"
            time.sleep(2.0 * (attempt + 1))

    return fname, 0, "failed (retries exceeded)"


def main():
    target_dir = TARGET_DIR
    target_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 65)
    print("      SYZYGY 3-4-5 PIECE VERIFIED MULTI-THREADED DOWNLOADER     ")
    print("=" * 65)
    print(f"Target Directory : {target_dir}")
    print(f"Source URL       : {BASE_URL}")

    try:
        checksums = fetch_checksums()
    except Exception as exc:
        print(f"[ERROR] Failed to fetch checksums: {exc}")
        return 1

    total_files = len(checksums)
    print(f"Found {total_files} tablebase files to verify and download.\n")

    workers = 3  # Polite concurrency to prevent HTTP 403 rate limits
    completed = 0
    total_bytes_downloaded = 0
    t0 = time.time()

    with ThreadPoolExecutor(max_workers=workers) as executor:
        future_map = {
            executor.submit(download_single_file, fname, md5, target_dir): fname
            for fname, md5 in checksums.items()
        }

        for future in as_completed(future_map):
            fname, fbytes, status = future.result()
            completed += 1
            total_bytes_downloaded += fbytes
            pct = (completed / total_files) * 100
            elapsed = time.time() - t0
            print(f"[{completed:3d}/{total_files}] ({pct:5.1f}%) {fname:<15} -> {status}")

    total_elapsed = time.time() - t0
    mb_downloaded = total_bytes_downloaded / (1024 * 1024)
    print("\n" + "=" * 65)
    print("                     DOWNLOAD COMPLETED                         ")
    print("=" * 65)
    print(f"Total Files       : {completed} / {total_files}")
    print(f"Data Processed    : {mb_downloaded:.1f} MB")
    print(f"Elapsed Time      : {total_elapsed:.1f} seconds")
    print(f"Location          : {target_dir}")
    print("=" * 65)
    return 0


if __name__ == "__main__":
    sys.exit(main())
