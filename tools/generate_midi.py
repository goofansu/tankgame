#!/usr/bin/env python3
import os
import struct

PPQ = 480
BPM = 110
BARS = 5
TEMPO_USEC = int(60_000_000 / BPM)


def var_len(value):
    value = int(value)
    buffer = value & 0x7F
    value >>= 7
    out = [buffer]
    while value:
        buffer = (value & 0x7F) | 0x80
        out.insert(0, buffer)
        value >>= 7
    return bytes(out)


def add_event(events, tick, data, order=0):
    events.append((tick, order, data))


def build_track(events):
    events.sort(key=lambda item: (item[0], item[1]))
    track = bytearray()
    prev_tick = 0
    for tick, _, data in events:
        delta = tick - prev_tick
        track.extend(var_len(delta))
        track.extend(data)
        prev_tick = tick
    track.extend(var_len(1))
    track.extend(b"\xFF\x2F\x00")
    return track


def write_midi(path, events):
    track = build_track(events)
    header = struct.pack(">4sLHHH", b"MThd", 6, 0, 1, PPQ)
    track_header = struct.pack(">4sL", b"MTrk", len(track))
    with open(path, "wb") as f:
        f.write(header)
        f.write(track_header)
        f.write(track)


def add_meta(events):
    add_event(events, 0, b"\xFF\x51\x03" + TEMPO_USEC.to_bytes(3, "big"), 0)
    add_event(events, 0, b"\xFF\x58\x04\x04\x02\x18\x08", 1)


def add_meta_with_bpm(events, bpm):
    tempo_usec = int(60_000_000 / bpm)
    add_event(events, 0, b"\xFF\x51\x03" + tempo_usec.to_bytes(3, "big"), 0)
    add_event(events, 0, b"\xFF\x58\x04\x04\x02\x18\x08", 1)


def pad_track(events, total_ticks):
    add_event(events, total_ticks, b"\xFF\x01\x00", 10)


def add_program(events, tick, channel, program):
    add_event(events, tick, bytes([0xC0 | channel, program]), 2)


def add_note(events, tick, channel, key, length, velocity=100):
    add_event(events, tick, bytes([0x90 | channel, key, velocity]), 3)
    add_event(events, tick + length, bytes([0x80 | channel, key, 64]), 4)


def build_drums():
    events = []
    add_meta(events)
    bar_ticks = 4 * PPQ
    note_len = PPQ // 8
    for bar in range(BARS):
        base = bar * bar_ticks
        if bar in (0, 3):
            add_note(events, base, 9, 49, note_len, 110)
        for beat in range(4):
            tick = base + beat * PPQ
            if beat in (0, 2):
                add_note(events, tick, 9, 36, note_len, 110)
            else:
                add_note(events, tick, 9, 38, note_len, 115)
        for hat in range(8):
            tick = base + hat * (PPQ // 2)
            add_note(events, tick, 9, 42, note_len, 70)
        if bar == BARS - 1:
            roll_base = base + 3 * PPQ
            for step in range(4):
                add_note(events, roll_base + step * (PPQ // 4), 9, 38, note_len, 90)
    pad_track(events, BARS * bar_ticks)
    return events


def build_tuba():
    events = []
    add_meta(events)
    add_program(events, 0, 0, 58)
    bar_ticks = 4 * PPQ
    half_note = 2 * PPQ
    progression = [36, 36, 41, 31, 36]
    fifths = [31, 31, 36, 38, 31]
    for bar, root in enumerate(progression[:BARS]):
        base = bar * bar_ticks
        add_note(events, base, 0, root, half_note, 100)
        add_note(events, base + half_note, 0, fifths[bar], half_note, 95)
    pad_track(events, BARS * bar_ticks)
    return events


def build_melody():
    events = []
    add_meta(events)
    add_program(events, 0, 1, 73)
    eighth = PPQ // 2
    phrase = [
        (72, 2 * eighth), (67, eighth), (67, eighth), (66, eighth), (67, eighth), (72, 2 * eighth),
        (67, eighth), (67, eighth), (66, eighth), (67, eighth), (72, 2 * eighth), (67, eighth), (72, eighth),
        (67, eighth), (72, eighth), (67, eighth), (64, eighth), (67, eighth), (72, 2 * eighth), (76, eighth),
        (79, 2 * eighth), (76, eighth), (74, eighth), (72, 2 * eighth), (67, eighth), (72, eighth),
        (64, eighth), (67, eighth), (72, 2 * eighth), (67, eighth), (72, eighth), (67, eighth), (72, eighth),
    ]
    tick = 0
    total_ticks = BARS * 4 * PPQ
    for key, length in phrase:
        if tick >= total_ticks:
            break
        remaining = total_ticks - tick
        final_len = int(min(length, remaining) * 0.9)
        add_note(events, tick, 1, key, final_len, 105)
        tick += length
    pad_track(events, total_ticks)
    return events


def build_victory():
    events = []
    add_meta_with_bpm(events, BPM * 2)
    add_program(events, 0, 2, 56)
    bar_ticks = 4 * PPQ
    half = 2 * PPQ
    quarter = PPQ
    fanfare = [
        (72, quarter), (76, quarter), (79, half),
        (76, quarter), (79, quarter), (84, half),
    ]
    tick = 0
    total_ticks = 2 * bar_ticks
    for key, length in fanfare:
        add_note(events, tick, 2, key, int(length * 0.9), 110)
        tick += length
    pad_track(events, total_ticks)
    return events


def main():
    out_dir = os.path.join("assets", "music", "march")
    os.makedirs(out_dir, exist_ok=True)

    write_midi(os.path.join(out_dir, "drums.mid"), build_drums())
    write_midi(os.path.join(out_dir, "bass.mid"), build_tuba())
    write_midi(os.path.join(out_dir, "melody.mid"), build_melody())
    write_midi(os.path.join(out_dir, "victory.mid"), build_victory())


if __name__ == "__main__":
    main()
