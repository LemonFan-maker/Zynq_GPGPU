#!/usr/bin/env python3
"""
MNIST FC baseline (NumPy only)
- Network: 784 -> hidden -> 10
- Loss: softmax cross-entropy
- Optimizer: mini-batch SGD
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parent
NPZ_DIR = ROOT / "npz"


def load_data(train_npz: Path, test_npz: Path, max_train: int | None, max_test: int | None):
    tr = np.load(train_npz)
    te = np.load(test_npz)

    x_train = tr["x"].astype(np.float32).reshape(-1, 28 * 28) / 255.0
    y_train = tr["y"].astype(np.int64)
    x_test = te["x"].astype(np.float32).reshape(-1, 28 * 28) / 255.0
    y_test = te["y"].astype(np.int64)

    if max_train is not None:
        x_train = x_train[:max_train]
        y_train = y_train[:max_train]
    if max_test is not None:
        x_test = x_test[:max_test]
        y_test = y_test[:max_test]

    return x_train, y_train, x_test, y_test


def one_hot(y: np.ndarray, num_classes: int = 10) -> np.ndarray:
    oh = np.zeros((y.shape[0], num_classes), dtype=np.float32)
    oh[np.arange(y.shape[0]), y] = 1.0
    return oh


def softmax(z: np.ndarray) -> np.ndarray:
    z = z - z.max(axis=1, keepdims=True)
    e = np.exp(z)
    return e / e.sum(axis=1, keepdims=True)


def relu(x: np.ndarray) -> np.ndarray:
    return np.maximum(0.0, x)


def relu_grad(x: np.ndarray) -> np.ndarray:
    return (x > 0).astype(np.float32)


def accuracy(logits: np.ndarray, y: np.ndarray) -> float:
    pred = np.argmax(logits, axis=1)
    return float((pred == y).mean())


def main():
    parser = argparse.ArgumentParser(description="MNIST FC baseline with NumPy")
    parser.add_argument("--train", type=Path, default=NPZ_DIR / "mnist_train.npz")
    parser.add_argument("--test", type=Path, default=NPZ_DIR / "mnist_test.npz")
    parser.add_argument("--hidden", type=int, default=128)
    parser.add_argument("--epochs", type=int, default=5)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--lr", type=float, default=0.08)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--max-train", type=int, default=None)
    parser.add_argument("--max-test", type=int, default=None)
    parser.add_argument("--save", type=Path, default=ROOT / "mnist_fc_weights.npz")
    args = parser.parse_args()

    np.random.seed(args.seed)

    x_train, y_train, x_test, y_test = load_data(
        args.train, args.test, args.max_train, args.max_test
    )

    n_train, in_dim = x_train.shape
    out_dim = 10
    hidden = args.hidden

    # Kaiming-like init for ReLU
    w1 = np.random.randn(in_dim, hidden).astype(np.float32) * np.sqrt(2.0 / in_dim)
    b1 = np.zeros((1, hidden), dtype=np.float32)
    w2 = np.random.randn(hidden, out_dim).astype(np.float32) * np.sqrt(2.0 / hidden)
    b2 = np.zeros((1, out_dim), dtype=np.float32)

    y_train_oh = one_hot(y_train, out_dim)

    print(f"[info] train={x_train.shape}, test={x_test.shape}, hidden={hidden}")
    print(
        f"[info] epochs={args.epochs}, batch={args.batch_size}, lr={args.lr}, seed={args.seed}"
    )

    t0 = time.perf_counter()

    for epoch in range(1, args.epochs + 1):
        # shuffle
        idx = np.random.permutation(n_train)
        x_train = x_train[idx]
        y_train_oh = y_train_oh[idx]

        running_loss = 0.0
        steps = 0

        for st in range(0, n_train, args.batch_size):
            ed = min(st + args.batch_size, n_train)
            xb = x_train[st:ed]
            yb = y_train_oh[st:ed]

            # forward
            z1 = xb @ w1 + b1
            a1 = relu(z1)
            z2 = a1 @ w2 + b2
            prob = softmax(z2)

            # loss
            eps = 1e-8
            loss = -np.mean(np.sum(yb * np.log(prob + eps), axis=1))
            running_loss += float(loss)
            steps += 1

            # backward
            m = xb.shape[0]
            dz2 = (prob - yb) / m
            dw2 = a1.T @ dz2
            db2 = dz2.sum(axis=0, keepdims=True)

            da1 = dz2 @ w2.T
            dz1 = da1 * relu_grad(z1)
            dw1 = xb.T @ dz1
            db1 = dz1.sum(axis=0, keepdims=True)

            # SGD update
            w2 -= args.lr * dw2
            b2 -= args.lr * db2
            w1 -= args.lr * dw1
            b1 -= args.lr * db1

        # evaluate per epoch
        tr_logits = relu(x_train @ w1 + b1) @ w2 + b2
        te_logits = relu(x_test @ w1 + b1) @ w2 + b2
        tr_acc = accuracy(tr_logits, np.argmax(y_train_oh, axis=1))
        te_acc = accuracy(te_logits, y_test)

        print(
            f"[epoch {epoch:02d}] loss={running_loss/max(1,steps):.4f} "
            f"train_acc={tr_acc*100:.2f}% test_acc={te_acc*100:.2f}%"
        )

    t1 = time.perf_counter()
    print(f"[done] total_time={t1 - t0:.2f}s")

    args.save.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        args.save,
        w1=w1,
        b1=b1,
        w2=w2,
        b2=b2,
        hidden=np.array([hidden], dtype=np.int32),
        input_dim=np.array([in_dim], dtype=np.int32),
        output_dim=np.array([out_dim], dtype=np.int32),
    )
    print(f"[ok] saved weights: {args.save}")


if __name__ == "__main__":
    main()
