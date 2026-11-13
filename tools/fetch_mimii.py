"""Download a small subset of the MIMII fan dataset into data/mimii/ (gitignored).

MIMII (Purohit et al., Hitachi), CC BY-SA 4.0: https://zenodo.org/records/3384388

The fan zip is ~10 GB, but Zenodo supports HTTP range requests, so this reads
the zip's index remotely and downloads only the clips it keeps: N normal +
N abnormal from machine id_00, evenly spaced. Each clip is downmixed to mono
(channel 0 of the 8-mic array), 16 kHz / 16-bit, to match the INMP441.

Usage: python3 tools/fetch_mimii.py [N]      (default N=100, ~0.5 GB transfer, ~65 MB on disk)
"""
import io
import struct
import subprocess
import sys
import time
import wave
import zipfile
from pathlib import Path

URL = "https://zenodo.org/records/3384388/files/6_dB_fan.zip?download=1"
MACHINE = "id_00"
OUT = Path(__file__).resolve().parent.parent / "data" / "mimii" / "fan_6dB_id_00"
CHUNK = 1 << 20  # read-ahead, so zipfile's small reads don't each become a request


class HttpRangeFile(io.RawIOBase):
    """Seekable read-only file backed by HTTP range requests."""

    def __init__(self, url):
        self.url, self.pos = url, 0
        self.buf_start, self.buf = 0, b""
        self.size = int(self._get(0, 0, want_total=True))

    def _get(self, start, end, want_total=False):
        # curl rather than urllib: it uses the macOS certificate store, so it
        # works even with Pythons that ship without CA certificates.
        # Retries are done here, not with curl --retry: after a dropped transfer,
        # curl's retry re-sends to stdout and duplicates the bytes already received.
        cmd = ["curl", "-sSfL", "--max-time", "120", "-r", f"{start}-{end}"]
        if want_total:
            cmd += ["-o", "/dev/null", "-D", "-"]
        for attempt in range(6):
            r = subprocess.run(cmd + [self.url], capture_output=True)
            if r.returncode == 0:
                if want_total:
                    for line in r.stdout.decode(errors="replace").splitlines():
                        if line.lower().startswith("content-range:"):
                            return line.rsplit("/", 1)[1].strip()
                    raise IOError("server ignored range request")
                if len(r.stdout) == end - start + 1:
                    return r.stdout
                err = f"got {len(r.stdout)} of {end - start + 1} bytes"
            else:
                err = r.stderr.decode(errors="replace").strip()
            print(f"\n  retry {attempt + 1}/5: {err}", file=sys.stderr)
            time.sleep(2 * (attempt + 1))
        raise IOError(f"range {start}-{end} failed: {err}")

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.pos

    def seek(self, off, whence=io.SEEK_SET):
        base = {io.SEEK_SET: 0, io.SEEK_CUR: self.pos, io.SEEK_END: self.size}[whence]
        self.pos = base + off
        return self.pos

    def read(self, n=-1):
        if n is None or n < 0:
            n = self.size - self.pos
        n = min(n, self.size - self.pos)
        if n <= 0:
            return b""
        lo, hi = self.buf_start, self.buf_start + len(self.buf)
        if not (lo <= self.pos and self.pos + n <= hi):
            end = min(self.pos + max(n, CHUNK), self.size) - 1
            self.buf_start, self.buf = self.pos, self._get(self.pos, end)
            lo = self.buf_start
        out = self.buf[self.pos - lo:self.pos - lo + n]
        self.pos += len(out)
        return out


def evenly_spaced(items, n):
    if len(items) <= n:
        return items
    step = len(items) / n
    return [items[int(i * step)] for i in range(n)]


def read_wav(raw: bytes):
    """Return (channels, sample_width, rate, pcm_bytes). Handles WAVE_FORMAT_EXTENSIBLE,
    which MIMII uses and Python's wave module (before 3.12) rejects."""
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("not a WAV file")
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(raw):
        cid, size = raw[pos:pos + 4], struct.unpack_from("<I", raw, pos + 4)[0]
        body = raw[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            _, ch, rate, _, _, bits = struct.unpack_from("<HHIIHH", body)
            fmt = (ch, bits // 8, rate)
        elif cid == b"data":
            data = body
        pos += 8 + size + (size & 1)
    if fmt is None or data is None:
        raise ValueError("WAV missing fmt or data chunk")
    return (*fmt, data)


def to_mono(raw: bytes, dst: Path) -> None:
    ch, width, rate, frames = read_wav(raw)
    step = ch * width
    mono = b"".join(frames[i:i + width] for i in range(0, len(frames), step))
    with wave.open(str(dst), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(width)
        w.setframerate(rate)
        w.writeframes(mono)


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    print("Reading zip index from zenodo.org...")
    zf = zipfile.ZipFile(HttpRangeFile(URL))
    names = zf.namelist()

    for label in ("normal", "abnormal"):
        clips = sorted(x for x in names if f"/{MACHINE}/{label}/" in x and x.endswith(".wav"))
        if not clips:
            sys.exit(f"no {MACHINE}/{label} clips found in the zip")
        picked = evenly_spaced(clips, n)
        dst_dir = OUT / label
        dst_dir.mkdir(parents=True, exist_ok=True)
        for i, name in enumerate(picked, 1):
            dst = dst_dir / Path(name).name
            if not dst.exists():
                to_mono(zf.read(name), dst)
            print(f"\r{label}: {i}/{len(picked)}", end="", flush=True)
        print(f"  (of {len(clips)} available) -> {dst_dir}")

    (OUT / "README.md").write_text(
        "# MIMII fan subset (6 dB SNR, machine id_00)\n\n"
        "Source: MIMII Dataset, Purohit et al. (Hitachi), https://zenodo.org/records/3384388\n"
        "License: CC BY-SA 4.0. Downmixed to mono (mic channel 0), 16 kHz / 16-bit.\n\n"
        "- `normal/`: healthy fan\n"
        "- `abnormal/`: faulty fan (imbalance, voltage change, clogging)\n"
    )


if __name__ == "__main__":
    main()
