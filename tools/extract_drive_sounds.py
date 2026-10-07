#!/usr/bin/env python3
"""Cut the bundled Disk II sounds out of SavageFX's recording
"Apple IIe Computer Boot and Load a ProDOS 5.25" Disk"
(https://freesound.org/s/388192/, CC-BY 4.0).

usage: extract_drive_sounds.py <recording.wav> <output dir>

The input must be mono 16-bit WAV; convert the Freesound preview with e.g.
    afconvert -f WAVE -d LEI16@44100 -c 1 388192_2605536-hq.mp3 recording.wav
Writes motor.wav (seamless 2 s loop), step1.wav, step2.wav and grind.wav.
Requires numpy.
"""
import os
import sys
import wave

import numpy as np

SR = 44100


def load(path):
    w = wave.open(path)
    rate = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(float) / 32768
    t = np.arange(int(len(x) * SR / rate)) * rate / SR
    return np.interp(t, np.arange(len(x)), x)


def save(path, x):
    w = wave.open(path, 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())


def main():
    x = load(sys.argv[1])
    out = sys.argv[2]

    # Head hits of the boot-time recalibration (first 1.7 s): onsets of the
    # 5 ms envelope of everything above 2 kHz
    spectrum = np.fft.rfft(x)
    spectrum[np.fft.rfftfreq(len(x), 1 / SR) < 2000] = 0
    highpass = np.fft.irfft(spectrum, len(x))
    k = int(SR * 0.005)
    env = np.sqrt(np.convolve(highpass ** 2, np.ones(k) / k, 'same'))
    threshold = 4 * np.median(env)
    hits, last = [], -SR
    for i in range(1, int(1.7 * SR)):
        if env[i] > threshold >= env[i - 1] and i - last > SR * 0.008:
            hits.append(i)
            last = i

    # Motor: the steadiest 2 s (lowest envelope variation) after the boot,
    # made loopable by crossfading its tail into its head
    best, best_var = 0.0, float('inf')
    for start in np.arange(2.0, 30.0, 0.1):
        seg = np.abs(x[int(start * SR):int((start + 2.0) * SR)])
        blocks = seg[:(len(seg) // 2205) * 2205].reshape(-1, 2205).mean(axis=1)
        var = blocks.std() / blocks.mean()
        if var < best_var:
            best, best_var = start, var
    motor = x[int(best * SR):int(best * SR) + 2 * SR].copy()
    motor -= motor.mean()
    fade = SR // 20
    motor[:fade] = motor[:fade] * np.linspace(0, 1, fade) + motor[-fade:] * np.linspace(1, 0, fade)
    motor = motor[:-fade]

    def cut(i, ms):
        s = x[max(0, i - int(0.002 * SR)):][:int(ms / 1000 * SR)].copy()
        s[:int(0.001 * SR)] *= np.linspace(0, 1, int(0.001 * SR))
        s[-int(0.006 * SR):] *= np.linspace(1, 0, int(0.006 * SR))
        return s

    # The recording has no isolated head steps, so the steps are shortened,
    # softer recalibration hits
    by_strength = sorted(hits, key=lambda i: env[i:i + int(0.01 * SR)].max(), reverse=True)
    grind = x[max(0, hits[0] - int(0.01 * SR)):hits[-1] + int(0.08 * SR)].copy()
    grind[-int(0.02 * SR):] *= np.linspace(1, 0, int(0.02 * SR))

    save(os.path.join(out, 'motor.wav'), motor)
    save(os.path.join(out, 'step1.wav'), cut(by_strength[3], 30) * 0.5)
    save(os.path.join(out, 'step2.wav'), cut(by_strength[5], 30) * 0.5)
    save(os.path.join(out, 'grind.wav'), grind)
    print(f"{len(hits)} recalibration hits; motor loop from {best:.1f} s")


if __name__ == '__main__':
    main()
