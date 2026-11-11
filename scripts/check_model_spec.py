#!/usr/bin/env python3
"""Reproduce planned DS-CNN shapes, kernel weights and multiply-accumulates.

SAME padding, NHWC, depth multiplier 1. Excludes bias, BN, activation,
feature extraction, pooling and softmax costs. No runtime/arena prediction.
"""
from pathlib import Path

frames = 1 + (24000 - 512) // 256
assert frames == 92
height, width, channels = frames, 32, 1
weights = macs = 0
rows = []


def conv(name, kernel, stride, outputs, depthwise=False):
    global height, width, channels, weights, macs
    height = (height + stride - 1) // stride
    width = (width + stride - 1) // stride
    count = kernel * kernel * channels * (1 if depthwise else outputs)
    weights += count
    macs += height * width * count
    channels = channels if depthwise else outputs
    rows.append((name, height, width, channels, count, height * width * count))


conv('stem', 3, 2, 16)
for n, (stride, outputs) in enumerate(((1, 32), (2, 32), (1, 64), (2, 64)), 1):
    conv(f'dw{n}', 3, stride, channels, True)
    conv(f'pw{n}', 1, 1, outputs)
weights += channels * 4
macs += channels * 4
assert weights == 9376
print('layer height width channels kernel_weights MACs')
for row in rows:
    print(*row)
print(f'Kernel weights: {weights:,}; MACs including dense: {macs:,}')
doc = (Path(__file__).resolve().parents[1] / 'doc/AUDIO_VISION.md').read_text()
assert f'`{weights:,}` kernel weights' in doc
assert f'`{macs:,}` MACs' in doc
assert '[1, 92, 32, 1]' in doc
