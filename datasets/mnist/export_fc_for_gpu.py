#!/usr/bin/env python3
"""
Export MNIST FC artifacts to GPU-friendly layout.

Inputs:
- mnist_fc_weights.npz: float32 FC weights from fc_mnist_baseline.py
- npz/mnist_test.npz: MNIST test set

Outputs (default: datasets/mnist/gpu_export):
- fc_gpu_bundle.npz containing:
  * quantized weights/biases
  * test samples and labels
  * 8x8 blocked matrices for tiled GEMM path
  * metadata for dequantization and padded shapes
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np


def symmetric_int8_quant(x: np.ndarray):
    max_abs = float(np.max(np.abs(x)))
    scale = max(max_abs / 127.0, 1e-12)
    q = np.clip(np.round(x / scale), -127, 127).astype(np.int8)
    return q, np.float32(scale)


def quantize_activation_u8(x01: np.ndarray):
    # input is expected in [0, 1]
    scale = np.float32(1.0 / 255.0)
    q = np.clip(np.round(x01 / scale), 0, 255).astype(np.uint8)
    return q, scale


def pad_2d(x: np.ndarray, r_mul: int, c_mul: int):
    r, c = x.shape
    rp = ((r + r_mul - 1) // r_mul) * r_mul
    cp = ((c + c_mul - 1) // c_mul) * c_mul
    out = np.zeros((rp, cp), dtype=x.dtype)
    out[:r, :c] = x
    return out


def block_2d(x: np.ndarray, br: int = 8, bc: int = 8):
    """
    Return blocks in row-major block order:
    shape -> [num_blocks, br, bc]
    """
    r, c = x.shape
    assert r % br == 0 and c % bc == 0
    blocks = (
        x.reshape(r // br, br, c // bc, bc)
        .transpose(0, 2, 1, 3)
        .reshape(-1, br, bc)
    )
    return blocks


def main():
    parser = argparse.ArgumentParser(description="Export FC baseline artifacts for GPU")
    parser.add_argument(
        "--weights",
        type=Path,
        default=Path(__file__).resolve().parent / "mnist_fc_weights.npz",
    )
    parser.add_argument(
        "--mnist-test",
        type=Path,
        default=Path(__file__).resolve().parent / "npz" / "mnist_test.npz",
    )
    parser.add_argument("--sample-count", type=int, default=128)
    parser.add_argument("--tile", type=int, default=8)
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "gpu_export",
    )
    parser.add_argument("--out-name", type=str, default="fc_gpu_bundle.npz")
    args = parser.parse_args()

    if args.tile <= 0:
        raise ValueError("--tile must be > 0")

    w = np.load(args.weights)
    w1 = w["w1"].astype(np.float32)  # [784, hidden]
    b1 = w["b1"].astype(np.float32).reshape(-1)  # [hidden]
    w2 = w["w2"].astype(np.float32)  # [hidden, 10]
    b2 = w["b2"].astype(np.float32).reshape(-1)  # [10]

    t = np.load(args.mnist_test)
    x_test = t["x"].astype(np.float32).reshape(-1, 28 * 28) / 255.0
    y_test = t["y"].astype(np.int64)

    n = min(args.sample_count, x_test.shape[0])
    x_sel = x_test[:n]
    y_sel = y_test[:n]

    # Quantize activations and weights
    x_q, x_scale = quantize_activation_u8(x_sel)
    w1_q, w1_scale = symmetric_int8_quant(w1)
    w2_q, w2_scale = symmetric_int8_quant(w2)

    # Bias to int32 in the same quant domain: y = (x_q*sx)*(w_q*sw)
    # sum domain scale per layer: sx*sw
    b1_q = np.round(b1 / (x_scale * w1_scale)).astype(np.int32)
    # hidden activation will be requantized later in runtime; keep b2 in float and int32 proxy
    b2_q = np.round(b2 / (w2_scale * max(1e-12, np.float32(1.0 / 127.0)))).astype(np.int32)

    tile = args.tile

    # Prepare matrices for tiled GEMM pipeline (row-major blocks)
    # Layer1: [N, 784] x [784, H]
    x_pad = pad_2d(x_q.astype(np.int16), tile, tile).astype(np.int16)
    w1_pad = pad_2d(w1_q.astype(np.int16), tile, tile).astype(np.int16)

    # Layer2 input is runtime-generated; we still export W2 in blocked form
    w2_pad = pad_2d(w2_q.astype(np.int16), tile, tile).astype(np.int16)

    x_blocks = block_2d(x_pad, tile, tile)
    w1_blocks = block_2d(w1_pad, tile, tile)
    w2_blocks = block_2d(w2_pad, tile, tile)

    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / args.out_name

    np.savez_compressed(
        out_path,
        # original model sizes
        in_dim=np.array([w1.shape[0]], dtype=np.int32),
        hidden_dim=np.array([w1.shape[1]], dtype=np.int32),
        out_dim=np.array([w2.shape[1]], dtype=np.int32),
        sample_count=np.array([n], dtype=np.int32),
        tile=np.array([tile], dtype=np.int32),
        # quant params
        x_scale=np.array([x_scale], dtype=np.float32),
        w1_scale=np.array([w1_scale], dtype=np.float32),
        w2_scale=np.array([w2_scale], dtype=np.float32),
        # quantized tensors
        x_q=x_q,
        y=y_sel,
        w1_q=w1_q,
        b1_q=b1_q,
        w2_q=w2_q,
        b2_q=b2_q,
        # padded / blocked tensors for tiled path
        x_pad=x_pad,
        w1_pad=w1_pad,
        w2_pad=w2_pad,
        x_blocks=x_blocks,
        w1_blocks=w1_blocks,
        w2_blocks=w2_blocks,
        x_pad_shape=np.array(x_pad.shape, dtype=np.int32),
        w1_pad_shape=np.array(w1_pad.shape, dtype=np.int32),
        w2_pad_shape=np.array(w2_pad.shape, dtype=np.int32),
    )

    print(f"[ok] exported: {out_path}")
    print(f"[info] x_q={x_q.shape}, y={y_sel.shape}, tile={tile}")
    print(f"[info] w1_q={w1_q.shape}, w2_q={w2_q.shape}")
    print(f"[info] x_blocks={x_blocks.shape}, w1_blocks={w1_blocks.shape}, w2_blocks={w2_blocks.shape}")


if __name__ == "__main__":
    main()
