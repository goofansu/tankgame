#!/usr/bin/env python3
import os
import struct

PPQ = 480
BPM = 120
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


def add_program(events, tick, channel, program):
    add_event(events, tick, bytes([0xC0 | channel, program]), 2)


def add_note(events, tick, channel, key, length, velocity=100):
    add_event(events, tick, bytes([0x90 | channel, key, velocity]), 3)
    add_event(events, tick + length, bytes([0x80 | channel, key, 64]), 4)


def build_drums():
    events = []
    add_meta(events)
    bar_ticks = 4 * PPQ
    note_len = PPQ // 4
    for bar in range(8):
        base = bar * bar_ticks
        for beat in range(4):
            tick = base + beat * PPQ
            if beat in (0, 2):
                add_note(events, tick, 9, 36, note_len, 110)
                add_note(events, tick, 9, 38, note_len, 110)
            else:
                add_note(events, tick, 9, 38, note_len, 100)
        for hat in range(8):
            tick = base + hat * (PPQ // 2)
            add_note(events, tick, 9, 42, note_len, 80)
    return events


def build_bass():
    events = []
    add_meta(events)
    add_program(events, 0, 0, 58)
    progression = [43, 43, 48, 48, 50, 50, 43, 43]
    bar_ticks = 4 * PPQ
    note_len = int(PPQ * 0.75)
    for bar, key in enumerate(progression):
        base = bar * bar_ticks
        for beat in range(4):
            add_note(events, base + beat * PPQ, 0, key, note_len, 90)
    return events


def build_melody():
    events = []
    add_meta(events)
    add_program(events, 0, 1, 56)
    motif = [67, 71, 74, 79, 79, 78, 76, 74, 72, 74, 76, 78, 79, 74, 71, 67]
    note_len = int(PPQ * 0.375)
    tick = 0
    for _ in range(4):
        for key in motif:
            add_note(events, tick, 1, key, note_len, 100)
            tick += PPQ // 2
    return events


def main():
    out_dir = os.path.join("assets", "music")
    os.makedirs(out_dir, exist_ok=True)

    write_midi(os.path.join(out_dir, "march_drums.mid"), build_drums())
    write_midi(os.path.join(out_dir, "march_bass.mid"), build_bass())
    write_midi(os.path.join(out_dir, "march_melody.mid"), build_melody())


if __name__ == "__main__":
    main()
