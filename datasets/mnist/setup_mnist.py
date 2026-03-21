#!/usr/bin/env python3
import gzip
import struct
import urllib.request
from pathlib import Path
import numpy as np

BASE_URL = "https://systemds.apache.org/assets/datasets/mnist"
FILES = [
    "train-images-idx3-ubyte.gz",
    "train-labels-idx1-ubyte.gz",
    "t10k-images-idx3-ubyte.gz",
    "t10k-labels-idx1-ubyte.gz",
]


def download(url: str, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.stat().st_size > 0:
        print(f"[skip] exists: {dst.name}")
        return
    print(f"[down] {url}")
    urllib.request.urlretrieve(url, dst)
    print(f"[ok]   {dst.name} ({dst.stat().st_size} bytes)")


def gunzip_file(src_gz: Path, dst_file: Path):
    if dst_file.exists() and dst_file.stat().st_size > 0:
        print(f"[skip] unzipped exists: {dst_file.name}")
        return
    print(f"[uzip] {src_gz.name} -> {dst_file.name}")
    with gzip.open(src_gz, "rb") as f_in, open(dst_file, "wb") as f_out:
        f_out.write(f_in.read())


def read_idx_images(path: Path):
    with open(path, "rb") as f:
        magic, n, rows, cols = struct.unpack(">IIII", f.read(16))
        if magic != 2051:
            raise ValueError(f"{path.name}: bad image magic {magic}")
        data = np.frombuffer(f.read(), dtype=np.uint8)
    return data.reshape(n, rows, cols)


def read_idx_labels(path: Path):
    with open(path, "rb") as f:
        magic, n = struct.unpack(">II", f.read(8))
        if magic != 2049:
            raise ValueError(f"{path.name}: bad label magic {magic}")
        data = np.frombuffer(f.read(), dtype=np.uint8)
    return data


def main():
    root = Path(__file__).resolve().parent
    raw_dir = root / "raw"
    idx_dir = root / "idx"
    npz_dir = root / "npz"
    raw_dir.mkdir(parents=True, exist_ok=True)
    idx_dir.mkdir(parents=True, exist_ok=True)
    npz_dir.mkdir(parents=True, exist_ok=True)

    # 1) download
    for name in FILES:
        url = f"{BASE_URL}/{name}"
        download(url, raw_dir / name)

    # 2) unzip
    for name in FILES:
        src = raw_dir / name
        dst = idx_dir / name[:-3]  # strip .gz
        gunzip_file(src, dst)

    # 3) parse + sanity check
    train_x = read_idx_images(idx_dir / "train-images-idx3-ubyte")
    train_y = read_idx_labels(idx_dir / "train-labels-idx1-ubyte")
    test_x = read_idx_images(idx_dir / "t10k-images-idx3-ubyte")
    test_y = read_idx_labels(idx_dir / "t10k-labels-idx1-ubyte")

    assert len(train_x) == len(train_y), "train image/label mismatch"
    assert len(test_x) == len(test_y), "test image/label mismatch"

    print(f"[info] train: images={train_x.shape}, labels={train_y.shape}")
    print(f"[info] test : images={test_x.shape}, labels={test_y.shape}")

    # 4) optional npz export
    np.savez_compressed(npz_dir / "mnist_train.npz", x=train_x, y=train_y)
    np.savez_compressed(npz_dir / "mnist_test.npz", x=test_x, y=test_y)
    print(f"[ok] saved: {npz_dir / 'mnist_train.npz'}")
    print(f"[ok] saved: {npz_dir / 'mnist_test.npz'}")


if __name__ == "__main__":
    main()
