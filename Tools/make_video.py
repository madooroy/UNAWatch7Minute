#!/usr/bin/env python3
"""Turn a Tools/sim_video.sh recording into the demo video: the watch in a
round bezel, a caption underneath, and the workout's beeps on the soundtrack.

Keeps the start and the end of the recorded workout (the middle six minutes
are cut), in real time. The beeps are rebuilt from the simulator's log
("Sim.Buzzer::play" lines), timed against the moment the workout was started,
so they line up with the screen. usage: Tools/make_video.py OUTDIR   (writes OUTDIR/SevenMinute_demo.mp4)
Needs ffmpeg and Pillow.
"""
import math, os, re, struct, subprocess, sys, wave
from PIL import Image, ImageDraw

OUT = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'Software/Apps/LVGL-GUI/assets/ttf/Poppins-Medium.ttf')
W, H, S = 720, 800, 600
X, Y = (W - S) // 2, 40
RATE = 44100

marks = {}
for line in open(os.path.join(OUT, 'marks.txt')):
    a, b = line.split()
    if a != 'audio_offset':
        marks[b] = float(a)

# Cue times from the log, on the video's clock (the workout start is the R1
# press at the "start" mark).
log = open(os.path.join(OUT, 'sim.log')).read().splitlines()
t_go = next(int(l.split()[0]) for l in log if 'Workout start' in l)   # ms, log clock
def vt(ms):
    return (ms - t_go) / 1000.0 + marks['start']
cues = [(vt(int(l.split()[0])), l.split('CUE ')[1].split()[0]) for l in log if ' CUE ' in l]
def cue(name, n=1, before=None):
    hits = [t for t, c in cues if c == name and (before is None or t < before)]
    return hits[n - 1] if n > 0 else hits[n]
GO1, REST1, GO2 = cue('GO'), cue('REST'), cue('GO', 2)
PAUSED, RESUMED = cue('PAUSED'), cue('RESUMED')
SWITCH, FINISHED = cue('SWITCH_SIDES'), cue('FINISHED')
LAST_REST, LAST_GO = cue('REST', -1, SWITCH), cue('GO', -1, SWITCH)

# The parts of the recording that are kept, in order (seconds on its clock).
WINDOWS = [
    (marks['home'] - 0.5, GO1 + 6.0),          # home, settings, get ready, GO
    (REST1 - 0.5, RESUMED + 2.5),              # rest with the next exercise, GO, pause
    (LAST_REST + 5.5, LAST_GO + 3.0),          # six minutes on: side plank next
    (SWITCH - 3.0, SWITCH + 3.0),              # switch sides
    (FINISHED - 1.5, marks['end']),            # fanfare and summary
]
CAPTIONS = [   # (from, to, text), same clock
    (marks['home'] - 0.5, marks['settings'], 'The scientific 7-minute workout'),
    (marks['settings'], marks['start'], 'Rounds and alerts, each next to its button'),
    (marks['start'], GO1, 'Get ready: 3-2-1 ticks, then GO'),
    (GO1, REST1 - 1, 'Work: 30 s, the ring drains'),
    (REST1 - 1, GO2, 'Rest: the next exercise and a tip'),
    (GO2, PAUSED, 'Every change has its own beep and buzz'),
    (PAUSED, RESUMED + 3, 'Pause any time'),
    (LAST_REST, LAST_GO, 'Six minutes on: the side plank'),
    (LAST_GO, SWITCH - 3.5, 'Every change has its own beep and buzz'),
    (SWITCH - 3.5, SWITCH + 4, 'Switch sides at 15 s: a triple chirp'),
    (FINISHED - 2, marks['end'] + 1, 'Done: a fanfare and the summary'),
]
T_START, T_END = WINDOWS[0][0], WINDOWS[-1][1]

# -- soundtrack from the log ------------------------------------------------------
melodies = {}
for l in log:
    m = re.search(r'^\s*(\d+) .*Sim\.Buzzer::play.*tone (\d+) ms (\d+) Hz level (\d+)', l)
    if m:
        t, ms, hz, lvl = map(int, m.groups())
        key = next((k for k in melodies if abs(k - t) <= 3), t)
        melodies.setdefault(key, []).append((ms, hz, lvl))
n = int((T_END - T_START) * RATE)
buf = [0.0] * n
for t_ms, notes in melodies.items():
    pos = vt(t_ms) - T_START
    for ms, hz, lvl in notes:
        i0, cnt = int(pos * RATE), int(ms / 1000.0 * RATE)
        if hz and lvl:
            amp = 0.12 * lvl
            for i in range(cnt):
                j = i0 + i
                if 0 <= j < n:
                    env = min(1.0, i / 200.0, (cnt - i) / 400.0)   # no clicks
                    ph = 2 * math.pi * hz * i / RATE
                    buf[j] += amp * env * (math.sin(ph) + 0.25 * math.sin(3 * ph))
        pos += ms / 1000.0
wav = os.path.join(OUT, 'beeps.wav')
with wave.open(wav, 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(b''.join(struct.pack('<h', max(-32767, min(32767, int(v * 32767)))) for v in buf))

# -- picture ----------------------------------------------------------------------
mask = Image.new('RGBA', (W, H), (18, 18, 18, 255))
d = ImageDraw.Draw(mask)
d.ellipse([X - 22, Y - 22, X + S + 21, Y + S + 21], fill=(44, 44, 44, 255))
d.ellipse([X, Y, X + S - 1, Y + S - 1], fill=(0, 0, 0, 0))
mask_png = os.path.join(OUT, 'mask.png')
mask.save(mask_png)

def esc(c):
    return c.replace(':', '\\:').replace("'", '’').replace(',', '\\,')
texts = ','.join(
    f"drawtext=fontfile={FONT}:text='{esc(c)}':fontsize=30:fontcolor=white:x=(w-tw)/2:y={Y + S + 60}"
    f":enable='between(t,{a - T_START:.2f},{b - T_START:.2f})'"
    for a, b, c in CAPTIONS)
n = len(WINDOWS)
fc = (f'[0:v]scale={S}:{S}[v];color=c=0x121212:s={W}x{H}:r=25[bg];[bg][v]overlay={X}:{Y}:shortest=1[o];'
      f'[o][1:v]overlay=0:0,{texts},format=yuv420p,split={n}' + ''.join(f'[v{i}]' for i in range(n)) + ';'
      f'[2:a]asplit={n}' + ''.join(f'[a{i}]' for i in range(n)) + ';')
for i, (a, b) in enumerate(WINDOWS):
    a, b = a - T_START, b - T_START
    fc += (f'[v{i}]trim={a:.3f}:{b:.3f},setpts=PTS-STARTPTS[vt{i}];'
           f'[a{i}]atrim={a:.3f}:{b:.3f},asetpts=PTS-STARTPTS[at{i}];')
fc += ''.join(f'[vt{i}][at{i}]' for i in range(n)) + f'concat=n={n}:v=1:a=1[outv][outa]'
final = os.path.join(OUT, 'SevenMinute_demo.mp4')
subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-ss', f'{T_START:.2f}', '-t', f'{T_END - T_START:.2f}',
                '-i', os.path.join(OUT, 'demo.mp4'), '-i', mask_png, '-i', wav,
                '-filter_complex', fc, '-map', '[outv]', '-map', '[outa]', '-c:v', 'libx264', '-crf', '20',
                '-c:a', 'aac', '-b:a', '128k', '-movflags', '+faststart', final], check=True)
os.remove(mask_png)
os.remove(wav)
print(final)
