#!/usr/bin/env python3
"""Cross-validate diagnostic AVT boundary estimators on A/B traces.

The traces contain ground truth and are not suitable for production decoding.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np


def load(path, quadratic=False):
    data = np.genfromtxt(path, delimiter=",", skip_header=1)
    if data.ndim != 2 or data.shape[1] != 16 or not np.isfinite(data).all():
        raise ValueError(f"invalid boundary trace: {path}")
    if not np.all((data[:, 2] >= 0) & (data[:, 2] <= 255)):
        raise ValueError(f"invalid target values: {path}")
    features = np.column_stack((np.ones(len(data)), (data[:, 3:] - 1900) / 400))
    if quadratic:
        terms = features[:, 1:]
        features = np.column_stack((features, *(terms[:, i] * terms[:, j]
                                               for i in range(terms.shape[1])
                                               for j in range(i, terms.shape[1]))))
    return features, data[:, 2]


def fit(features, target, ridge):
    if ridge:
        penalty = np.eye(features.shape[1]) * math.sqrt(ridge)
        penalty[0, 0] = 0.0  # Do not penalise the intercept.
        features = np.concatenate((features, penalty))
        target = np.concatenate((target, np.zeros(penalty.shape[0])))
    return np.linalg.lstsq(features, target, rcond=1e-8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--quadratic", action="store_true")
    parser.add_argument("--ridge", type=float, default=0.0)
    args = parser.parse_args()
    if not math.isfinite(args.ridge) or not 0 <= args.ridge <= 1000:
        parser.error("--ridge must be finite and between 0 and 1000")
    kinds = ("gradient", "smooth", "extreme")
    def path(kind, side):
        return args.directory / f"decodium-avt-90-qrm-{kind}-{side}.csv"
    results = []
    for trained in kinds:
        x, y = load(path(trained, "a"), args.quadratic)
        coef, _, rank, _ = fit(x, y, args.ridge)
        for tested in kinds:
            z, wanted = load(path(tested, "b"), args.quadratic)
            # Avoid BLAS-specific floating point status flags leaking from
            # otherwise finite matrix operations into NumPy warnings.
            with np.errstate(all="ignore"):
                estimated = np.clip(z @ coef, 0, 255)
            if not np.isfinite(estimated).all():
                raise ValueError("non-finite fitted result")
            error = np.abs(estimated - wanted)
            results.append({"trained": trained + "-a", "tested": tested + "-b",
                            "rank": int(rank), "count": len(error),
                            "mae": float(error.mean()), "maximum": float(error.max()),
                            "over_20": int((error > 20).sum())})
    # Train only on A for all families. The three B traces remain unseen;
    # repeat in the opposite direction to reveal asymmetry.
    for train_side, test_side in (("a", "b"), ("b", "a")):
        training = [load(path(kind, train_side), args.quadratic) for kind in kinds]
        x = np.concatenate([item[0] for item in training])
        y = np.concatenate([item[1] for item in training])
        coef, _, rank, _ = fit(x, y, args.ridge)
        for tested in (*kinds, "random"):
            z, wanted = load(path(tested, test_side), args.quadratic)
            with np.errstate(all="ignore"):
                estimated = np.clip(z @ coef, 0, 255)
            if not np.isfinite(estimated).all():
                raise ValueError("non-finite fitted result")
            error = np.abs(estimated - wanted)
            results.append({"trained": "combined-" + train_side, "tested": tested + "-" + test_side,
                            "rank": int(rank), "count": len(error),
                            "mae": float(error.mean()), "maximum": float(error.max()),
                            "over_20": int((error > 20).sum())})
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
