"""Generates 100% original procedural WAV SFX (14) and seamless adaptive music loops (3)
for BLACK NUMBERS under Saved/GeneratedAudio/ using pure Python + numpy + wave,
and imports them into UE5 under /Game/Audio/SFX and /Game/Audio/Music.

Standalone WAV generation (system python3):
    python3 Tools/bn_audio_assets.py --generate-only
UE5 import (run via UnrealEditor-Cmd -run=pythonscript):
    UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/bn_audio_assets.py
"""
import math
import os
import subprocess
import sys
import wave

SR = 44100
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedAudio")
SFX_DIR = os.path.join(OUT_DIR, "SFX")
MUSIC_DIR = os.path.join(OUT_DIR, "Music")

UE_SFX_DIR = "/Game/Audio/SFX"
UE_MUSIC_DIR = "/Game/Audio/Music"

SFX_NAMES = [
    "SFX_Crash_Light",
    "SFX_Crash_Heavy",
    "SFX_Break_Wood",
    "SFX_Break_Metal",
    "SFX_Breaker_Collapse",
    "SFX_UI_Hover",
    "SFX_UI_Click",
    "SFX_UI_Confirm",
    "SFX_UI_Error",
    "SFX_Countdown_Beep",
    "SFX_Countdown_Go",
    "SFX_Radio_Squelch",
    "SFX_Nos_Ignite",
    "SFX_NearMiss_Whoosh",
    "SFX_Shift_Pop",
    "SFX_Engine_Blown",
    "SFX_Gps_Arrive",
    "SFX_Camera_Flash",
    "SFX_Garage_Repair",
    "SFX_New_Record",
    "SFX_Rain_Ambient",
]

MUSIC_NAMES = [
    "MUS_FreeRoam_Karamara",
    "MUS_Race_Adrenaline",
    "MUS_Pursuit_Heat",
]


def _write_wav(path, samples, peak=0.88):
    import numpy as np

    os.makedirs(os.path.dirname(path), exist_ok=True)
    arr = np.asarray(samples, dtype=np.float64)
    arr = np.nan_to_num(arr, nan=0.0, posinf=0.0, neginf=0.0)
    mx = float(np.max(np.abs(arr))) if arr.size else 1.0
    if mx > 1e-6:
        arr = arr * (peak / mx)
    pcm = np.clip(arr * 32767.0, -32768.0, 32767.0).astype(np.int16)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(pcm.tobytes())


def _lowpass(x, cutoff_hz):
    import numpy as np

    # Fast FFT brickwall/smooth filter (100% deterministic & vectorized)
    n = len(x)
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    spec = np.fft.rfft(x)
    resp = 1.0 / np.sqrt(1.0 + (freqs / max(cutoff_hz, 20.0)) ** 4)
    return np.fft.irfft(spec * resp, n=n)


def _bandpass(x, lo_hz, hi_hz):
    import numpy as np

    n = len(x)
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    spec = np.fft.rfft(x)
    hp = 1.0 / np.sqrt(1.0 + (max(lo_hz, 10.0) / np.maximum(freqs, 1.0)) ** 4)
    lp = 1.0 / np.sqrt(1.0 + (freqs / max(hi_hz, lo_hz + 20.0)) ** 4)
    return np.fft.irfft(spec * hp * lp, n=n)


def _fade_edges(x, attack_s=0.004, release_s=0.015):
    import numpy as np

    out = x.copy()
    na = min(int(attack_s * SR), len(out) // 2)
    nr = min(int(release_s * SR), len(out) // 2)
    if na > 1:
        out[:na] *= np.linspace(0.0, 1.0, na)
    if nr > 1:
        out[-nr:] *= np.linspace(1.0, 0.0, nr)
    return out


def _make_sfx():
    import numpy as np

    rng = np.random.default_rng(2026)

    def t_arr(dur):
        return np.arange(int(dur * SR), dtype=np.float64) / SR

    # 1. SFX_Crash_Light
    t = t_arr(0.42)
    noise = rng.normal(0.0, 1.0, len(t))
    thump = np.sin(2 * np.pi * (110.0 * np.exp(-t * 14.0) + 42.0) * t) * np.exp(-t * 15.0)
    crunch = _bandpass(noise, 450.0, 2600.0) * np.exp(-t * 11.0)
    scrape = np.sin(2 * np.pi * 640.0 * t + 2.5 * np.sin(2 * np.pi * 83.0 * t)) * np.exp(-t * 16.0)
    _write_wav(os.path.join(SFX_DIR, "SFX_Crash_Light.wav"), _fade_edges(0.75 * thump + 0.65 * crunch + 0.25 * scrape))

    # 2. SFX_Crash_Heavy
    t = t_arr(0.85)
    noise = rng.normal(0.0, 1.0, len(t))
    boom = np.tanh(2.4 * np.sin(2 * np.pi * (95.0 * np.exp(-t * 8.0) + 32.0) * t)) * np.exp(-t * 6.5)
    metal = _bandpass(noise, 320.0, 3200.0) * np.exp(-t * 6.0)
    glass = _bandpass(rng.normal(0.0, 1.0, len(t)), 4200.0, 9500.0) * np.exp(-np.maximum(t - 0.04, 0.0) * 9.0) * (t > 0.03)
    clang = (np.sin(2 * np.pi * 290 * t) + 0.7 * np.sin(2 * np.pi * 515 * t) + 0.5 * np.sin(2 * np.pi * 810 * t)) * np.exp(-t * 8.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_Crash_Heavy.wav"), _fade_edges(0.95 * boom + 0.75 * metal + 0.35 * glass + 0.30 * clang))

    # 3. SFX_Break_Wood
    t = t_arr(0.42)
    sig = np.zeros_like(t)
    for offset, f0, amp in ((0.0, 420.0, 1.0), (0.045, 680.0, 0.75), (0.095, 310.0, 0.65), (0.14, 890.0, 0.45)):
        dt = np.maximum(t - offset, 0.0)
        mask = (t >= offset).astype(np.float64)
        snap = _bandpass(rng.normal(0.0, 1.0, len(t)), f0 * 0.6, f0 * 2.4) * np.exp(-dt * 28.0) * mask
        wood_body = np.sin(2 * np.pi * f0 * dt) * np.exp(-dt * 34.0) * mask
        sig += amp * (0.75 * snap + 0.45 * wood_body)
    _write_wav(os.path.join(SFX_DIR, "SFX_Break_Wood.wav"), _fade_edges(sig))

    # 4. SFX_Break_Metal
    t = t_arr(0.54)
    clang = (
        0.70 * np.sin(2 * np.pi * 345.0 * t) * np.exp(-t * 8.0)
        + 0.55 * np.sin(2 * np.pi * 592.0 * t) * np.exp(-t * 9.5)
        + 0.40 * np.sin(2 * np.pi * 940.0 * t) * np.exp(-t * 11.0)
        + 0.30 * np.sin(2 * np.pi * 1510.0 * t) * np.exp(-t * 14.0)
    )
    rattle = _bandpass(rng.normal(0.0, 1.0, len(t)), 700.0, 3800.0) * np.exp(-t * 10.0)
    _write_wav(os.path.join(SFX_DIR, "SFX_Break_Metal.wav"), _fade_edges(0.85 * clang + 0.45 * rattle))

    # 5. SFX_Breaker_Collapse (dramatic Most Wanted structural collapse!)
    t = t_arr(1.65)
    n1 = rng.normal(0.0, 1.0, len(t))
    n2 = rng.normal(0.0, 1.0, len(t))
    snap = _bandpass(n1, 600.0, 4000.0) * np.exp(-t * 14.0)
    rumble = _lowpass(n2, 165.0) * np.exp(-np.maximum(t - 0.12, 0.0) * 2.2) * np.minimum(t / 0.08, 1.0)
    sub_drop = np.tanh(2.5 * np.sin(2 * np.pi * (82.0 * np.exp(-t * 2.8) + 26.0) * t)) * np.exp(-t * 2.6)
    debris = np.zeros_like(t)
    for k in range(7):
        off = 0.18 + k * 0.16
        dt = np.maximum(t - off, 0.0)
        m = (t >= off).astype(np.float64)
        debris += (0.85 ** k) * _bandpass(rng.normal(0.0, 1.0, len(t)), 180.0, 1600.0) * np.exp(-dt * 12.0) * m
    _write_wav(os.path.join(SFX_DIR, "SFX_Breaker_Collapse.wav"), _fade_edges(0.65 * snap + 1.0 * rumble + 0.90 * sub_drop + 0.55 * debris, release_s=0.08))

    # 6. SFX_UI_Hover
    t = t_arr(0.065)
    freq = np.linspace(1100.0, 1480.0, len(t))
    phase = 2 * np.pi * np.cumsum(freq) / SR
    sig = np.sin(phase) * np.exp(-t * 55.0) * 0.55
    _write_wav(os.path.join(SFX_DIR, "SFX_UI_Hover.wav"), _fade_edges(sig, 0.002, 0.01), peak=0.45)

    # 7. SFX_UI_Click
    t = t_arr(0.095)
    body = np.sin(2 * np.pi * (240.0 * np.exp(-t * 32.0) + 120.0) * t) * np.exp(-t * 38.0)
    tick = _bandpass(rng.normal(0.0, 1.0, len(t)), 1600.0, 4800.0) * np.exp(-t * 70.0)
    _write_wav(os.path.join(SFX_DIR, "SFX_UI_Click.wav"), _fade_edges(0.75 * body + 0.55 * tick, 0.001, 0.01), peak=0.68)

    # 8. SFX_UI_Confirm
    t = t_arr(0.24)
    a = (np.sin(2 * np.pi * 440.0 * t) + 0.35 * np.sin(2 * np.pi * 880.0 * t)) * np.exp(-t * 16.0)
    dt = np.maximum(t - 0.065, 0.0)
    b = (t >= 0.065) * (np.sin(2 * np.pi * 659.25 * dt) + 0.30 * np.sin(2 * np.pi * 1318.5 * dt)) * np.exp(-dt * 12.0)
    _write_wav(os.path.join(SFX_DIR, "SFX_UI_Confirm.wav"), _fade_edges(0.65 * a + 0.85 * b, 0.002, 0.02), peak=0.72)

    # 9. SFX_UI_Error
    t = t_arr(0.22)
    buzz = np.tanh(2.0 * (np.sin(2 * np.pi * 155.0 * t) + 0.8 * np.sin(2 * np.pi * 192.0 * t)))
    env = ((t < 0.085).astype(np.float64) + (t >= 0.11).astype(np.float64)) * np.exp(-t * 6.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_UI_Error.wav"), _fade_edges(buzz * env, 0.003, 0.02), peak=0.65)

    # 10. SFX_Countdown_Beep
    t = t_arr(0.26)
    tone = np.sin(2 * np.pi * 620.0 * t) + 0.25 * np.sin(2 * np.pi * 1240.0 * t) + 0.10 * np.sin(2 * np.pi * 1860.0 * t)
    env = np.minimum(t / 0.008, 1.0) * np.exp(-t * 11.0)
    _write_wav(os.path.join(SFX_DIR, "SFX_Countdown_Beep.wav"), _fade_edges(tone * env, 0.002, 0.02), peak=0.80)

    # 11. SFX_Countdown_Go
    t = t_arr(0.58)
    chord = (
        0.65 * np.sin(2 * np.pi * 1240.0 * t)
        + 0.45 * np.sin(2 * np.pi * 930.0 * t)
        + 0.35 * np.sin(2 * np.pi * 620.0 * t)
        + 0.50 * np.sin(2 * np.pi * (160.0 * np.exp(-t * 7.0) + 55.0) * t) * np.exp(-t * 8.0)
    )
    env = np.minimum(t / 0.006, 1.0) * np.exp(-t * 4.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_Countdown_Go.wav"), _fade_edges(chord * env, 0.002, 0.04), peak=0.86)

    # 12. SFX_Radio_Squelch (police VHF burst + roger beep)
    t = t_arr(0.28)
    static_burst = _bandpass(rng.normal(0.0, 1.0, len(t)), 1100.0, 3200.0)
    gate = ((t < 0.11).astype(np.float64) * np.exp(-t * 9.0)) + 0.45 * ((t > 0.18).astype(np.float64) * np.exp(-(t - 0.18) * 18.0))
    beep = ((t >= 0.08) & (t <= 0.16)).astype(np.float64) * np.sin(2 * np.pi * 1850.0 * t) * 0.45
    _write_wav(os.path.join(SFX_DIR, "SFX_Radio_Squelch.wav"), _fade_edges(0.75 * static_burst * gate + beep, 0.003, 0.015), peak=0.62)

    # 13. SFX_Nos_Ignite
    t = t_arr(0.65)
    hiss = _bandpass(rng.normal(0.0, 1.0, len(t)), 900.0, 6200.0) * np.minimum(t / 0.03, 1.0) * np.exp(-t * 3.8)
    sweep_f = 240.0 + 1450.0 * np.exp(-((t - 0.11) / 0.14) ** 2)
    sweep_p = 2 * np.pi * np.cumsum(sweep_f) / SR
    turbine = np.sin(sweep_p) * 0.32 * np.exp(-t * 3.6)
    punch = np.sin(2 * np.pi * (140.0 * np.exp(-t * 10.0) + 52.0) * t) * np.exp(-t * 9.5) * 0.60
    _write_wav(os.path.join(SFX_DIR, "SFX_Nos_Ignite.wav"), _fade_edges(hiss + turbine + punch, 0.004, 0.04), peak=0.82)

    # 14. SFX_NearMiss_Whoosh (Doppler pass-by)
    t = t_arr(0.42)
    env = np.exp(-((t - 0.19) / 0.095) ** 2)
    wind = _bandpass(rng.normal(0.0, 1.0, len(t)), 260.0, 2100.0) * env
    doppler_f = 780.0 / (1.0 + 2.4 * t) + 140.0
    doppler = np.sin(2 * np.pi * np.cumsum(doppler_f) / SR) * env * 0.30
    _write_wav(os.path.join(SFX_DIR, "SFX_NearMiss_Whoosh.wav"), _fade_edges(wind + doppler, 0.01, 0.03), peak=0.72)

    # 15. SFX_Shift_Pop (punchy exhaust backfire crackle + turbo blow-off flutter)
    t = t_arr(0.32)
    n1 = rng.normal(0.0, 1.0, len(t))
    pop_thump = np.tanh(2.4 * np.sin(2 * np.pi * (170.0 * np.exp(-t * 28.0) + 52.0) * t)) * np.exp(-t * 24.0)
    crackle = _bandpass(n1, 550.0, 3600.0) * (np.exp(-t * 26.0) + 0.45 * np.exp(-np.maximum(t - 0.055, 0.0) * 34.0) * (t >= 0.055))
    bov_flutter = _bandpass(rng.normal(0.0, 1.0, len(t)), 1600.0, 4800.0) * (0.55 + 0.45 * np.sin(2 * np.pi * 24.0 * t)) * np.exp(-t * 10.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_Shift_Pop.wav"), _fade_edges(0.85 * pop_thump + 0.72 * crackle + 0.48 * bov_flutter, 0.002, 0.02), peak=0.86)

    # 16. SFX_Engine_Blown (metallic rod snap + piston clatter + pressurized radiator steam hiss)
    t = t_arr(1.35)
    snap = np.tanh(3.0 * np.sin(2 * np.pi * (140.0 * np.exp(-t * 14.0) + 38.0) * t)) * np.exp(-t * 9.0)
    clank = (np.sin(2 * np.pi * 410.0 * t) + 0.7 * np.sin(2 * np.pi * 735.0 * t) + 0.5 * np.sin(2 * np.pi * 1180.0 * t)) * np.exp(-t * 11.0)
    steam = _bandpass(rng.normal(0.0, 1.0, len(t)), 1400.0, 7200.0) * np.minimum(np.maximum(t - 0.08, 0.0) / 0.10, 1.0) * np.exp(-np.maximum(t - 0.18, 0.0) * 1.9)
    _write_wav(os.path.join(SFX_DIR, "SFX_Engine_Blown.wav"), _fade_edges(0.95 * snap + 0.65 * clank + 0.60 * steam, 0.002, 0.06), peak=0.88)

    # 17. SFX_Gps_Arrive (crisp 3-note futuristic navigation arrival chime)
    t = t_arr(0.45)
    chime = (
        0.60 * np.sin(2 * np.pi * 587.33 * t) * np.exp(-t * 14.0)
        + 0.70 * (t >= 0.08) * np.sin(2 * np.pi * 880.0 * np.maximum(t - 0.08, 0.0)) * np.exp(-np.maximum(t - 0.08, 0.0) * 11.0)
        + 0.80 * (t >= 0.16) * np.sin(2 * np.pi * 1174.66 * np.maximum(t - 0.16, 0.0)) * np.exp(-np.maximum(t - 0.16, 0.0) * 8.5)
    )
    _write_wav(os.path.join(SFX_DIR, "SFX_Gps_Arrive.wav"), _fade_edges(chime, 0.002, 0.03), peak=0.74)

    # 18. SFX_Camera_Flash (crisp speed-trap mechanical DSLR shutter + xenon strobe capacitor whine)
    t = t_arr(0.36)
    click1 = _bandpass(rng.normal(0.0, 1.0, len(t)), 1800.0, 7500.0) * np.exp(-t * 85.0)
    dt2 = np.maximum(t - 0.042, 0.0)
    click2 = (t >= 0.042) * _bandpass(rng.normal(0.0, 1.0, len(t)), 2200.0, 8200.0) * np.exp(-dt2 * 70.0) * 0.75
    whine_f = np.linspace(1400.0, 4200.0, len(t))
    whine = (t >= 0.05) * np.sin(2 * np.pi * np.cumsum(whine_f) / SR) * np.exp(-np.maximum(t - 0.05, 0.0) * 9.5) * 0.28
    _write_wav(os.path.join(SFX_DIR, "SFX_Camera_Flash.wav"), _fade_edges(0.95 * click1 + 0.80 * click2 + whine, 0.001, 0.02), peak=0.84)

    # 19. SFX_Garage_Repair (pneumatic impact wrench burst + hydraulic bay chime)
    t = t_arr(0.68)
    wrench = np.zeros_like(t)
    for k in range(5):
        off = 0.02 + k * 0.075
        dt = np.maximum(t - off, 0.0)
        m = (t >= off).astype(np.float64)
        wrench += m * (np.sin(2 * np.pi * 320.0 * dt) * 0.55 + _bandpass(rng.normal(0.0, 1.0, len(t)), 1200.0, 4800.0) * 0.65) * np.exp(-dt * 42.0)
    dt_c = np.maximum(t - 0.36, 0.0)
    chime_r = (t >= 0.36) * (0.65 * np.sin(2 * np.pi * 523.25 * dt_c) + 0.55 * np.sin(2 * np.pi * 783.99 * dt_c)) * np.exp(-dt_c * 7.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_Garage_Repair.wav"), _fade_edges(0.78 * wrench + 0.72 * chime_r, 0.002, 0.04), peak=0.82)

    # 20. SFX_New_Record (bright 4-note Most Wanted personal-best fanfare)
    t = t_arr(0.62)
    rec = np.zeros_like(t)
    for off, hz in ((0.0, 587.33), (0.09, 739.99), (0.18, 880.0), (0.27, 1174.66)):
        dt = np.maximum(t - off, 0.0)
        m = (t >= off).astype(np.float64)
        rec += m * (0.75 * np.sin(2 * np.pi * hz * dt) + 0.25 * np.sin(2 * np.pi * hz * 2.0 * dt)) * np.exp(-dt * 7.5)
    _write_wav(os.path.join(SFX_DIR, "SFX_New_Record.wav"), _fade_edges(rec, 0.002, 0.05), peak=0.80)

    # 21. SFX_Rain_Ambient (soft filtered rainfall noise bed)
    t = t_arr(2.0)
    rain = _bandpass(rng.normal(0.0, 1.0, len(t)), 550.0, 5400.0) * 0.65 + _lowpass(rng.normal(0.0, 1.0, len(t)), 420.0) * 0.25
    _write_wav(os.path.join(SFX_DIR, "SFX_Rain_Ambient.wav"), _fade_edges(rain, 0.08, 0.08), peak=0.52)


def _midi_hz(midi_note):
    return 440.0 * (2.0 ** ((midi_note - 69.0) / 12.0))


def _saw(phase):
    import numpy as np

    return 2.0 * (phase - np.floor(phase + 0.5))


def _make_music():
    import numpy as np

    rng = np.random.default_rng(707)

    def add_drum_kick(buf, start_idx, dur_s=0.22, f_hi=145.0, f_lo=44.0, amp=0.85):
        n = min(int(dur_s * SR), len(buf) - start_idx)
        if n <= 0:
            return
        t = np.arange(n, dtype=np.float64) / SR
        f = f_lo + (f_hi - f_lo) * np.exp(-t * 34.0)
        ph = 2 * np.pi * np.cumsum(f) / SR
        sig = np.tanh(1.8 * np.sin(ph)) * np.exp(-t * 14.0) * amp
        buf[start_idx:start_idx + n] += sig

    def add_drum_snare(buf, start_idx, dur_s=0.18, amp=0.55):
        n = min(int(dur_s * SR), len(buf) - start_idx)
        if n <= 0:
            return
        t = np.arange(n, dtype=np.float64) / SR
        body = np.sin(2 * np.pi * 190.0 * t) * np.exp(-t * 26.0)
        noise = _bandpass(rng.normal(0.0, 1.0, n), 1200.0, 6800.0) * np.exp(-t * 18.0)
        buf[start_idx:start_idx + n] += amp * (0.55 * body + 0.85 * noise)

    def add_hihat(buf, start_idx, dur_s=0.055, amp=0.20):
        n = min(int(dur_s * SR), len(buf) - start_idx)
        if n <= 0:
            return
        t = np.arange(n, dtype=np.float64) / SR
        hat = _bandpass(rng.normal(0.0, 1.0, n), 5800.0, 12500.0) * np.exp(-t * 58.0)
        buf[start_idx:start_idx + n] += amp * hat

    def add_tom(buf, start_idx, f0=115.0, dur_s=0.24, amp=0.50):
        n = min(int(dur_s * SR), len(buf) - start_idx)
        if n <= 0:
            return
        t = np.arange(n, dtype=np.float64) / SR
        f = f0 * (0.68 + 0.32 * np.exp(-t * 18.0))
        sig = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 11.0) * amp
        buf[start_idx:start_idx + n] += sig

    def make_seamless(buf):
        # Short 30 ms circular crossfade at loop seam guarantees click-free looping in UE5
        xf = int(0.030 * SR)
        if len(buf) > xf * 4:
            w = np.linspace(0.0, 1.0, xf)
            buf[:xf] = buf[:xf] * w + buf[-xf:] * (1.0 - w)
            buf = buf[:-xf]
        return buf

    # 1. MUS_FreeRoam_Karamara — 96 BPM, 4 bars (~10.0 s), dark noir D minor breakbeat
    bpm = 96.0
    step_s = 60.0 / bpm / 4.0  # 16th note
    n_steps = 64
    total_n = int(round(n_steps * step_s * SR)) + int(0.030 * SR)
    t = np.arange(total_n, dtype=np.float64) / SR
    buf = np.zeros(total_n, dtype=np.float64)

    bass_notes = [38, 38, 34, 36]  # D2, D2, Bb1, C2 (1 bar each)
    pad_chords = [
        (50, 53, 57, 62),  # Dm
        (50, 53, 57, 60),  # Dm7
        (46, 50, 53, 58),  # Bbmaj
        (48, 52, 55, 60),  # C
    ]
    for bar in range(4):
        s0 = int(bar * 16 * step_s * SR)
        s1 = min(int((bar + 1) * 16 * step_s * SR), total_n)
        tb = t[s0:s1] - t[s0]
        f_bass = _midi_hz(bass_notes[bar])
        reese = 0.55 * _saw(f_bass * tb) + 0.55 * _saw(f_bass * 1.006 * tb)
        lfo = 0.5 + 0.5 * np.sin(2 * np.pi * 0.5 * tb)
        buf[s0:s1] += 0.26 * (_lowpass(reese, 190.0) * (1.0 - lfo) + _lowpass(reese, 390.0) * lfo)

        pad = np.zeros(s1 - s0, dtype=np.float64)
        for m in pad_chords[bar]:
            fm = _midi_hz(m)
            pad += np.sin(2 * np.pi * fm * tb) + 0.25 * np.sin(2 * np.pi * fm * 2.0 * tb)
        pad_env = np.minimum(tb / 0.12, 1.0) * np.minimum((tb[-1] - tb + 0.01) / 0.15, 1.0)
        buf[s0:s1] += 0.095 * pad * pad_env

    for step in range(n_steps):
        idx = int(round(step * step_s * SR))
        s16 = step % 16
        if s16 in (0, 6, 10):
            add_drum_kick(buf, idx, amp=0.68)
        if s16 in (4, 12):
            add_drum_snare(buf, idx, amp=0.40)
        if s16 % 2 == 0:
            add_hihat(buf, idx, amp=0.14 if s16 % 4 == 0 else 0.08)
        # Subtle dark arpeggio pluck
        if s16 in (0, 3, 6, 10, 14):
            arp_m = [62, 65, 69, 65, 60][step % 5]
            ln = min(int(0.18 * SR), total_n - idx)
            ta = np.arange(ln, dtype=np.float64) / SR
            buf[idx:idx + ln] += 0.10 * np.sin(2 * np.pi * _midi_hz(arp_m) * ta) * np.exp(-ta * 14.0)

    _write_wav(os.path.join(MUSIC_DIR, "MUS_FreeRoam_Karamara.wav"), make_seamless(buf), peak=0.72)

    # 2. MUS_Race_Adrenaline — 134 BPM, 4 bars (~7.16 s), Most Wanted rock-electronic driving loop (E minor)
    bpm = 134.0
    step_s = 60.0 / bpm / 4.0
    n_steps = 64
    total_n = int(round(n_steps * step_s * SR)) + int(0.030 * SR)
    buf = np.zeros(total_n, dtype=np.float64)

    prog_roots = [40, 40, 36, 38]  # E2, E2, C2, D2
    arp_scale = [52, 55, 59, 64, 62, 59, 55, 57]  # Em pentatonic/aeolian drive
    for step in range(n_steps):
        idx = int(round(step * step_s * SR))
        bar = step // 16
        s16 = step % 16
        # Driving drums
        if s16 in (0, 3, 6, 8, 10, 14):
            add_drum_kick(buf, idx, f_hi=165.0, f_lo=48.0, amp=0.78 if s16 in (0, 8) else 0.56)
        if s16 in (4, 12):
            add_drum_snare(buf, idx, amp=0.62)
        add_hihat(buf, idx, amp=0.18 if s16 % 2 == 0 else 0.10)

        # Gritty rock-synth bass 8th/16th chug
        if s16 % 2 == 0:
            ln = min(int(step_s * 1.85 * SR), total_n - idx)
            ta = np.arange(ln, dtype=np.float64) / SR
            fb = _midi_hz(prog_roots[bar] + (12 if s16 in (6, 14) else 0))
            bass = np.tanh(2.4 * (_saw(fb * ta) + 0.5 * np.sin(2 * np.pi * fb * ta)))
            buf[idx:idx + ln] += 0.25 * _lowpass(bass, 480.0) * np.exp(-ta * 7.5)

        # Distorted power-chord guitar stab on bar beats
        if s16 in (0, 6, 12):
            ln = min(int(0.24 * SR), total_n - idx)
            ta = np.arange(ln, dtype=np.float64) / SR
            r = prog_roots[bar] + 12
            chord = _saw(_midi_hz(r) * ta) + 0.85 * _saw(_midi_hz(r + 7) * ta) + 0.65 * _saw(_midi_hz(r + 12) * ta)
            buf[idx:idx + ln] += 0.16 * _bandpass(np.tanh(2.8 * chord), 260.0, 2600.0) * np.exp(-ta * 9.0)

        # High-energy 16th synth arp
        ln = min(int(step_s * 1.15 * SR), total_n - idx)
        ta = np.arange(ln, dtype=np.float64) / SR
        fa = _midi_hz(arp_scale[step % len(arp_scale)] + (0 if bar < 2 else -2))
        lead = _saw(fa * ta) + 0.4 * np.sin(2 * np.pi * fa * 2.0 * ta)
        buf[idx:idx + ln] += 0.11 * _lowpass(lead, 2400.0) * np.exp(-ta * 15.0)

    _write_wav(os.path.join(MUSIC_DIR, "MUS_Race_Adrenaline.wav"), make_seamless(buf), peak=0.76)

    # 3. MUS_Pursuit_Heat — 146 BPM, 4 bars (~6.58 s), intense industrial tactical chase (F# minor)
    bpm = 146.0
    step_s = 60.0 / bpm / 4.0
    n_steps = 64
    total_n = int(round(n_steps * step_s * SR)) + int(0.030 * SR)
    buf = np.zeros(total_n, dtype=np.float64)

    chase_roots = [42, 42, 43, 40]  # F#2, F#2, G2 (Phrygian tension!), E2
    for step in range(n_steps):
        idx = int(round(step * step_s * SR))
        bar = step // 16
        s16 = step % 16

        if s16 in (0, 2, 6, 8, 11, 14):
            add_drum_kick(buf, idx, f_hi=175.0, f_lo=46.0, amp=0.80 if s16 in (0, 8) else 0.55)
        if s16 in (4, 12):
            add_drum_snare(buf, idx, amp=0.65)
        if s16 in (3, 7, 10, 15):
            add_tom(buf, idx, f0=125.0 if s16 < 8 else 96.0, amp=0.42)
        add_hihat(buf, idx, amp=0.20 if s16 % 2 == 1 else 0.12)

        # Relentless 16th industrial tension bass
        ln = min(int(step_s * 1.10 * SR), total_n - idx)
        ta = np.arange(ln, dtype=np.float64) / SR
        fb = _midi_hz(chase_roots[bar])
        pulse = np.tanh(3.2 * (_saw(fb * ta) + 0.55 * _saw(fb * 1.008 * ta)))
        buf[idx:idx + ln] += 0.24 * _lowpass(pulse, 560.0 if s16 % 4 == 0 else 320.0) * np.exp(-ta * 10.0)

        # Orchestral brass/string ostinato stabs (Most Wanted police tension!)
        if s16 in (0, 3, 6, 8, 12):
            ln = min(int(0.22 * SR), total_n - idx)
            ta = np.arange(ln, dtype=np.float64) / SR
            r = chase_roots[bar] + 12
            brass = _saw(_midi_hz(r) * ta) + 0.9 * _saw(_midi_hz(r + 7) * ta) + 0.7 * _saw(_midi_hz(r + 15) * ta)
            buf[idx:idx + ln] += 0.18 * _bandpass(np.tanh(2.2 * brass), 240.0, 2200.0) * np.exp(-ta * 9.5)

    _write_wav(os.path.join(MUSIC_DIR, "MUS_Pursuit_Heat.wav"), make_seamless(buf), peak=0.78)


def generate_all_wavs():
    _make_sfx()
    _make_music()
    print(f"BN: generated {len(SFX_NAMES)} SFX + {len(MUSIC_NAMES)} music WAVs under {OUT_DIR}")


def _all_wavs_exist():
    for name in SFX_NAMES:
        if not os.path.isfile(os.path.join(SFX_DIR, name + ".wav")):
            return False
    for name in MUSIC_NAMES:
        if not os.path.isfile(os.path.join(MUSIC_DIR, name + ".wav")):
            return False
    return True


def import_to_ue():
    import unreal

    if not _all_wavs_exist():
        res = subprocess.run(["python3", os.path.abspath(__file__), "--generate-only"], check=False)
        if res.returncode != 0:
            raise RuntimeError("bn_audio_assets.py --generate-only failed")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.EditorAssetLibrary

    def do_import(names, src_dir, dest_dir, loop_flag):
        tasks = []
        for name in names:
            wav_path = os.path.join(src_dir, name + ".wav")
            task = unreal.AssetImportTask()
            task.filename = wav_path
            task.destination_path = dest_dir
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = True
            task.factory = unreal.SoundFactory()
            tasks.append(task)
        asset_tools.import_asset_tasks(tasks)

        imported = 0
        for name in names:
            asset_path = f"{dest_dir}/{name}"
            sw = unreal.load_asset(asset_path)
            if sw is None:
                raise RuntimeError("Failed to import SoundWave: " + asset_path)
            if loop_flag:
                sw.set_editor_property("looping", True)
                lib.save_loaded_asset(sw, only_if_is_dirty=False)
            imported += 1
        return imported

    n_sfx = do_import(SFX_NAMES, SFX_DIR, UE_SFX_DIR, False)
    n_mus = do_import(MUSIC_NAMES, MUSIC_DIR, UE_MUSIC_DIR, True)
    unreal.log(f"BN: built {n_sfx} SFX + {n_mus} looping music SoundWaves under /Game/Audio/")


if __name__ == "__main__":
    if "--generate-only" in sys.argv:
        generate_all_wavs()
    else:
        import_to_ue()
