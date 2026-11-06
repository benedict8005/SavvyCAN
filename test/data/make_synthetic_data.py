#!/usr/bin/env python3
"""Generate the synthetic CAN test data in test/data/synthetic.

Everything is invented. Nothing comes from a real vehicle, person or company. The frames and
ground-truth files are deterministic. Some file headers record the time of generation, so the
files are not byte-identical between runs.

    python3 test/data/make_synthetic_data.py                     # writes test/data/synthetic
    python3 test/data/make_synthetic_data.py --large 2000000     # also writes a large candump log

Needs: python-can (>= 4.0) and cantools.
"""
import argparse
import json
import math
import os
import random

import can
import cantools
from cantools.database.can import Database, Message, Signal
from cantools.database.conversion import BaseConversion

HERE = os.path.dirname(os.path.abspath(__file__))
SEED = 20260101
DURATION = 10.0  # seconds covered by the main sample logs


# ----------------------------------------------------------------------------- databases
def sig(name, start, length, scale=1.0, offset=0.0, lo=None, hi=None, unit="", signed=False,
        byte_order="little_endian"):
    return Signal(name=name, start=start, length=length, byte_order=byte_order, is_signed=signed,
                  conversion=BaseConversion.factory(scale=scale, offset=offset), minimum=lo, maximum=hi,
                  unit=unit)


def build_database(variant="a"):
    """variant 'a' is the reference database. 'changed' differs in known ways (see DBC_DIFF)."""
    engine_signals = [
        sig("EngineSpeed", 0, 16, 0.25 if variant == "a" else 0.5, 0, 0, 8000, "rpm"),
        sig("CoolantTemp", 24, 8, 1, -40, -40, 215, "degC" if variant == "a" else "C"),
    ]
    if variant == "a":
        engine_signals.append(sig("Throttle", 16, 8, 0.4, 0, 0, 100, "%"))
    else:
        engine_signals.append(sig("EngineLoad", 32, 8, 0.5, 0, 0, 100, "%"))
    speed_start = 0 if variant == "a" else 8
    messages = [
        Message(0x100, "EngineData", 8, engine_signals, cycle_time=10, senders=["ECU_Engine"]),
        Message(0x200, "VehicleSpeed", 8, [
            sig("Speed", speed_start, 16, 0.01, 0, 0, 300, "km/h"),
            sig("MsgCounter", 48, 4, 1, 0, 0, 15),
            sig("Checksum", 56, 8, 1, 0, 0, 255),
        ], cycle_time=20, senders=["ECU_Chassis"]),
        Message(0x300 if variant == "a" else 0x301, "BatteryStatus", 8, [
            sig("Voltage", 0, 16, 0.1, 0, 0, 500, "V"),
            sig("Current", 16, 16, 0.1, 0, -300, 300, "A", signed=True),
            sig("StateOfCharge", 32, 8, 0.5, 0, 0, 100, "%"),
        ], cycle_time=100, senders=["ECU_Battery"]),
    ]
    if variant != "a":
        messages.append(Message(0x500, "Diagnostics", 8,
                                [sig("FaultCount", 0, 8, 1, 0, 0, 255)], cycle_time=1000,
                                senders=["ECU_Engine"]))
    return Database(messages=messages)


def build_clash_symbol_database():
    """Used as b.sym. ID 0x100 clashes with a.dbc (different name and layout). 0x600 is unique."""
    return Database(messages=[
        Message(0x100, "MotorStatus", 8, [
            sig("MotorTorque", 0, 12, 0.5, -1000, -1000, 1000, "Nm"),
            sig("MotorTemp", 16, 8, 1, -40, -40, 215, "degC"),
        ], cycle_time=10),
        Message(0x600, "Climate", 8, [
            sig("CabinTemp", 0, 8, 0.5, -20, -20, 60, "degC"),
            sig("FanLevel", 8, 4, 1, 0, 0, 10),
        ], cycle_time=500),
    ])


# ----------------------------------------------------------------------------- signal model
def engine_values(t, phase=0.0):
    return {
        "EngineSpeed": 2000 + 800 * math.sin(2 * math.pi * (t + phase) / 10.0),
        "Throttle": 20 + 10 * math.sin(2 * math.pi * (t + phase) / 5.0),
        "CoolantTemp": 85 + 5 * math.sin(2 * math.pi * (t + phase) / 60.0),
    }


def speed_values(t, counter):
    return {"Speed": 60 + 30 * math.sin(2 * math.pi * t / 8.0), "MsgCounter": counter % 16,
            "Checksum": 0}


def battery_values(t, phase=0.0):
    return {
        "Voltage": 380 + 5 * math.sin(2 * math.pi * (t + phase) / 20.0),
        "Current": 40 * math.sin(2 * math.pi * (t + phase) / 7.0),
        "StateOfCharge": 80 - 0.5 * (t + phase),
    }


def encode(db, name, values):
    msg = db.get_message_by_name(name)
    data = bytearray(msg.encode(values, scaling=True, strict=False))
    if name == "VehicleSpeed":  # checksum = XOR of bytes 0..6
        x = 0
        for b in data[:7]:
            x ^= b
        data[7] = x
    return bytes(data), msg


def decode_phys(db, name, data):
    return db.get_message_by_name(name).decode(bytes(data), scaling=True)


# ----------------------------------------------------------------------------- frame builders
def m(t, arb, data, ch=0, ext=False, fd=False, brs=False, esi=False, err=False):
    return can.Message(timestamp=t, arbitration_id=arb, data=data, channel=ch,
                       is_extended_id=ext, is_fd=fd, bitrate_switch=brs,
                       error_state_indicator=esi, is_error_frame=err, is_rx=True)


def periodic(db, name, period, ch, t0, t1, values_fn, arb=None):
    out = []
    n = int(round((t1 - t0) / period))
    for i in range(n):
        t = t0 + i * period
        v = values_fn(t, i)
        data, msg = encode(db, name, v)
        out.append(m(round(t, 6), arb if arb is not None else msg.frame_id, data, ch))
    return out


def main_sample(db):
    """3 channels (0, 1, 2 in python-can, shown as 1, 2, 3 in ASC)."""
    f = []
    f += periodic(db, "EngineData", 0.010, 0, 0, DURATION, lambda t, i: engine_values(t))
    f += periodic(db, "VehicleSpeed", 0.020, 0, 0, DURATION, lambda t, i: speed_values(t, i))
    f += periodic(db, "BatteryStatus", 0.100, 0, 0, DURATION, lambda t, i: battery_values(t))
    # channel 2 reuses IDs 0x100 and 0x300 with different values, to test per-bus decoding
    f += periodic(db, "EngineData", 0.010, 1, 0.003, DURATION, lambda t, i: engine_values(t, 2.5))
    f += periodic(db, "BatteryStatus", 0.100, 1, 0.003, DURATION, lambda t, i: battery_values(t, 5))
    # channel 3 carries only 0x200
    f += periodic(db, "VehicleSpeed", 0.020, 2, 0.007, DURATION, lambda t, i: speed_values(t + 1, i))
    f.sort(key=lambda x: (x.timestamp, x.channel))
    return f


def fault_sample(db):
    """Channel 0 only, with planted faults at known times."""
    f = []
    # 0x100: cycle drifts from 10 ms to 12 ms after t=6 s; EngineSpeed stuck 7.0 to 8.0 s;
    # Throttle spike (100 %) at 3.0 s
    t = 0.0
    while t < DURATION:
        v = engine_values(t)
        if 7.0 <= t < 8.0:
            v["EngineSpeed"] = 1500.0
        if 2.995 <= t < 3.005:
            v["Throttle"] = 100.0
        data, msg = encode(db, "EngineData", v)
        f.append(m(round(t, 6), msg.frame_id, data))
        t += 0.010 if t < 6.0 else 0.012
    # 0x200: dropout from 4.0 to 4.5 s
    for fr in periodic(db, "VehicleSpeed", 0.020, 0, 0, DURATION, lambda t, i: speed_values(t, i)):
        if not (4.0 <= fr.timestamp < 4.5):
            f.append(fr)
    # 0x300: normal
    f += periodic(db, "BatteryStatus", 0.100, 0, 0, DURATION, lambda t, i: battery_values(t))
    f.sort(key=lambda x: x.timestamp)
    return f


def can_fd_sample():
    rng = random.Random(SEED)
    f, lengths = [], []
    t = 0.0
    sizes = [8, 12, 16, 20, 24, 32, 48, 64]
    for i in range(400):
        t = round(i * 0.005, 6)
        size = sizes[i % len(sizes)]
        fd = size > 8 or i % 5 == 0
        ext = i % 7 == 0
        arb = (0x18DA0000 + (i % 4)) if ext else (0x300 + (i % 4))
        data = bytes(rng.randrange(256) for _ in range(size))
        f.append(m(t, arb, data, ch=0, ext=ext, fd=fd, brs=fd and i % 2 == 0, esi=fd and i % 11 == 0))
        lengths.append(size)
    return f, lengths


def error_frame_sample(db):
    f = periodic(db, "EngineData", 0.010, 0, 0, 8.0, lambda t, i: engine_values(t))
    bursts = [(2.0, 3), (4.0, 1), (6.0, 10)]
    errs = []
    for t0, n in bursts:
        for k in range(n):
            e = m(round(t0 + k * 0.001 + 0.0005, 6), 0x20000004, bytes([0, 0, 0, 0, 0, 0, 0, 0]),
                  err=True, ext=True)
            errs.append(e)
    f += errs
    f.sort(key=lambda x: x.timestamp)
    return f, bursts


# ----------------------------------------------------------------------------- J1939 and ISO-TP
def j1939_id(pri, pgn, sa, da=0xFF):
    if (pgn & 0xFF00) < 0xF000:  # PDU1: destination in the low byte
        pgn = (pgn & 0x3FF00) | da
    return (pri << 26) | (pgn << 8) | sa


def dtc(spn, fmi, oc):
    return bytes([spn & 0xFF, (spn >> 8) & 0xFF, ((spn >> 16) & 0x7) << 5 | (fmi & 0x1F), oc & 0x7F])


def dm1(dtcs):
    return bytes([0x04, 0xFF]) + b"".join(dtc(*d) for d in dtcs)


def tp_frames(t, sa, da, pgn, payload, mode, order=None, drop=(), step=0.05):
    """Frames of one J1939 transport transfer. mode: 'bam' or 'rts'."""
    n = (len(payload) + 6) // 7
    pgn_b = bytes([pgn & 0xFF, (pgn >> 8) & 0xFF, (pgn >> 16) & 0xFF])
    frames = []
    cm_id = j1939_id(7, 0xEC00, sa, da if mode == "rts" else 0xFF)
    dt_id = j1939_id(7, 0xEB00, sa, da if mode == "rts" else 0xFF)
    size = len(payload)
    if mode == "bam":
        frames.append(m(t, cm_id, bytes([32, size & 0xFF, size >> 8, n, 0xFF]) + pgn_b, ext=True))
        t += step
    else:
        frames.append(m(t, cm_id, bytes([16, size & 0xFF, size >> 8, n, 0xFF]) + pgn_b, ext=True))
        t += 0.01
        cts_id = j1939_id(7, 0xEC00, da, sa)
        frames.append(m(t, cts_id, bytes([17, n, 1, 0xFF, 0xFF]) + pgn_b, ext=True))
        t += 0.01
    seqs = order if order else list(range(1, n + 1))
    for s in seqs:
        chunk = payload[(s - 1) * 7:(s - 1) * 7 + 7] if 1 <= s <= n else b""
        chunk = chunk + bytes([0xFF] * (7 - len(chunk)))
        if s not in drop:
            frames.append(m(round(t, 6), dt_id, bytes([s]) + chunk, ext=True))
        t += step if mode == "bam" else 0.005
    if mode == "rts" and not drop and not order:
        eom_id = j1939_id(7, 0xEC00, da, sa)
        frames.append(m(round(t, 6), eom_id, bytes([19, size & 0xFF, size >> 8, n, 0xFF]) + pgn_b,
                        ext=True))
    return frames


def j1939_sample():
    f, truth = [], []
    d3 = [(110, 3, 1), (157, 18, 2), (3216, 4, 1)]
    d5 = [(100, 1, 1), (190, 0, 3), (91, 2, 1), (168, 3, 5), (520, 12, 2)]
    cases = [
        ("T1", 1.0, 0x00, 0xFF, "bam", dm1(d3), None, (), "complete", d3),
        ("T2", 2.0, 0x0B, 0xFF, "bam", dm1(d5), None, (), "complete", d5),
        ("T3", 3.0, 0x00, 0x17, "rts", dm1(d3), None, (), "complete", d3),
        ("T4", 5.0, 0x00, 0xFF, "bam", dm1(d5), None, (3,), "incomplete", d5),
        ("T5", 6.0, 0x0B, 0xFF, "bam", dm1(d5), [1, 3, 2, 4], (), "corrupted", d5),
    ]
    for name, t, sa, da, mode, payload, order, drop, status, dtcs in cases:
        frames = tp_frames(t, sa, da, 0xFECA, payload, mode, order, drop)
        f += frames
        truth.append({"name": name, "start_time": t, "source": sa, "destination": da,
                      "mode": mode, "pgn": 0xFECA, "length": len(payload), "status": status,
                      "payload_hex": payload.hex(),
                      "dtcs": [{"spn": a, "fmi": b, "occurrence": c} for a, b, c in dtcs]})
    # T6: request-to-send that never gets a clear-to-send (aborted by timeout)
    f.append(m(7.0, j1939_id(7, 0xEC00, 0x00, 0x17), bytes([16, 14, 0, 2, 0xFF, 0xCA, 0xFE, 0x00]), ext=True))
    truth.append({"name": "T6", "start_time": 7.0, "source": 0, "destination": 0x17, "mode": "rts",
                  "pgn": 0xFECA, "length": 14, "status": "aborted", "payload_hex": "", "dtcs": []})
    # single-frame traffic (EEC1-like, PGN 0xF004) every 10 ms from source 0
    for i in range(0, 1000):
        t = round(i * 0.010, 6)
        rpm = int((2000 + 500 * math.sin(2 * math.pi * t / 10)) / 0.125)
        f.append(m(t, j1939_id(3, 0xF004, 0x00), bytes([0xFF, 0xFF, 0xFF, rpm & 0xFF, rpm >> 8, 0xFF, 0xFF, 0xFF]), ext=True))
    f.sort(key=lambda x: x.timestamp)
    return f, truth


def isotp_transfer(t, req_id, rsp_id, payload, order=None, drop=(), pad=0x00, block=0):
    """Sender uses rsp_id, flow control comes from req_id. Returns frames."""
    frames = []
    n = len(payload)
    if n <= 7:
        data = bytes([n]) + payload
        frames.append(m(t, rsp_id, data + bytes([pad] * (8 - len(data)))))
        return frames
    first = bytes([0x10 | (n >> 8), n & 0xFF]) + payload[:6]
    frames.append(m(t, rsp_id, first))
    frames.append(m(round(t + 0.0005, 6), req_id, bytes([0x30, block, 0x00] + [pad] * 5)))
    rest = payload[6:]
    chunks = [rest[i:i + 7] for i in range(0, len(rest), 7)]
    seqs = order if order else [((i + 1) & 0x0F) for i in range(len(chunks))]
    tt = t + 0.001
    for i, s in enumerate(seqs):
        if (i + 1) in drop:
            tt += 0.001
            continue
        chunk = chunks[i] if i < len(chunks) else b""
        chunk = chunk + bytes([pad] * (7 - len(chunk)))
        frames.append(m(round(tt, 6), rsp_id, bytes([0x20 | (s & 0x0F)]) + chunk))
        tt += 0.001
    return frames


def isotp_sample():
    f, truth = [], []
    REQ, RSP = 0x7E0, 0x7E8

    def add(name, t, request, response, status, resp_frames=None, note=""):
        req_frames = isotp_transfer(t, RSP, REQ, request)  # request is single frame in practice
        f.extend(req_frames)
        f.extend(resp_frames if resp_frames is not None else isotp_transfer(t + 0.01, REQ, RSP, response))
        truth.append({"name": name, "start_time": t, "request_hex": request.hex(),
                      "response_hex": response.hex(), "status": status, "note": note})

    ident = b"SYNTH-ECU-0001"
    add("I1_read_did", 1.0, bytes([0x22, 0xF1, 0x90]), bytes([0x62, 0xF1, 0x90]) + ident, "complete",
        note="3-byte request, 17-byte multi-frame response")
    add("I2_negative_response", 2.0, bytes([0x22, 0xF1, 0x91]), bytes([0x7F, 0x22, 0x31]), "complete",
        note="UDS negative response, requestOutOfRange")
    long = bytes([0x62, 0xF1, 0x8C]) + bytes((i * 7 + 3) & 0xFF for i in range(97))
    add("I3_long_response", 3.0, bytes([0x22, 0xF1, 0x8C]), long, "complete",
        note="100-byte response, 14 consecutive frames")
    part = bytes([0x62, 0xF1, 0x92]) + bytes(range(17))
    add("I4_interrupted", 4.0, bytes([0x22, 0xF1, 0x92]), part, "incomplete",
        resp_frames=isotp_transfer(4.01, REQ, RSP, part, drop=(2,)),
        note="second consecutive frame is missing")
    add("I5_wrong_sequence", 5.0, bytes([0x22, 0xF1, 0x93]), part, "corrupted",
        resp_frames=isotp_transfer(5.01, REQ, RSP, part, order=[1, 3, 2]),
        note="consecutive frames carry sequence numbers 1, 3, 2")
    # steady background traffic, starting at 0 s so that every file format, which stores times
    # relative to the first frame, keeps the start times in the truth file
    for i in range(70):
        f.append(m(round(i * 0.1, 6), 0x1A0, bytes([i & 0xFF, 0, 0, 0, 0, 0, 0, 0])))
    f.sort(key=lambda x: x.timestamp)
    return f, truth


# ----------------------------------------------------------------------------- writers
def write_asc(path, frames):
    with can.ASCWriter(path) as w:
        for fr in frames:
            w.on_message_received(fr)


def write_blf(path, frames):
    with can.BLFWriter(path) as w:
        for fr in frames:
            w.on_message_received(fr)


def write_trc(path, frames):
    with can.TRCWriter(path) as w:
        for fr in frames:
            w.on_message_received(fr)
    # python-can writes the absolute path of the output file into the header; keep only the file
    # name so that no local directory names end up in the shipped files
    with open(path, newline="") as f:
        text = f.read()
    text = text.replace(os.path.abspath(path), os.path.basename(path)).replace(path, os.path.basename(path))
    with open(path, "w", newline="") as f:
        f.write(text)


def write_candump(path, frames, channel="vcan0"):
    with can.CanutilsLogWriter(path, channel=channel) as w:
        for fr in frames:
            w.on_message_received(fr)


def write_csv(path, frames):
    with can.CSVWriter(path) as w:
        for fr in frames:
            w.on_message_received(fr)


def frames_to_jsonable(frames):
    return [[fr.timestamp, fr.channel, fr.arbitration_id, bool(fr.is_extended_id), bool(fr.is_fd),
             bool(fr.is_error_frame), fr.data.hex()] for fr in frames]


def nominal_bits(fr):
    # frame length without stuffing bits, for a nominal bus-load figure
    return (67 if fr.is_extended_id else 47) + 8 * len(fr.data)


# ----------------------------------------------------------------------------- main
def mkdir(*p):
    d = os.path.join(*p)
    os.makedirs(d, exist_ok=True)
    return d


def write_json(path, obj):
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(obj, fh, indent=2, sort_keys=True)
        fh.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "synthetic"))
    ap.add_argument("--large", type=int, default=0, help="also write a candump log with N frames")
    a = ap.parse_args()
    out = a.out
    db = build_database("a")
    dbc_dir = mkdir(out, "databases")
    cantools.database.dump_file(db, os.path.join(dbc_dir, "a.dbc"))
    cantools.database.dump_file(db, os.path.join(dbc_dir, "a.sym"), database_format="sym")
    dbc_changed = build_database("changed")
    cantools.database.dump_file(dbc_changed, os.path.join(dbc_dir, "a_changed.dbc"))
    clash = build_clash_symbol_database()
    cantools.database.dump_file(clash, os.path.join(dbc_dir, "b.sym"), database_format="sym")
    write_json(os.path.join(dbc_dir, "truth_dbc_diff.json"), {
        "old": "a.dbc", "new": "a_changed.dbc",
        "changes": [
            {"kind": "signal_scale_changed", "message": "EngineData", "signal": "EngineSpeed",
             "old": 0.25, "new": 0.5, "alters_decoding": True},
            {"kind": "signal_removed", "message": "EngineData", "signal": "Throttle",
             "alters_decoding": True},
            {"kind": "signal_added", "message": "EngineData", "signal": "EngineLoad",
             "alters_decoding": False},
            {"kind": "signal_unit_changed", "message": "EngineData", "signal": "CoolantTemp",
             "old": "degC", "new": "C", "alters_decoding": False},
            {"kind": "signal_start_bit_changed", "message": "VehicleSpeed", "signal": "Speed",
             "old": 0, "new": 8, "alters_decoding": True},
            {"kind": "message_id_changed", "message": "BatteryStatus", "old": 0x300, "new": 0x301,
             "alters_decoding": True},
            {"kind": "message_added", "message": "Diagnostics", "id": 0x500, "alters_decoding": False},
        ]})
    write_json(os.path.join(dbc_dir, "truth_clashes.json"), {
        "files": ["a.dbc", "b.sym"],
        "clashes": [{"id": 0x100, "names": ["EngineData", "MotorStatus"]}],
        "unique_to_b": [{"id": 0x600, "name": "Climate"}]})

    # ---- main sample (classic CAN, 3 channels) in every writable format
    main_dir = mkdir(out, "classic")
    frames = main_sample(db)
    write_asc(os.path.join(main_dir, "sample.asc"), frames)
    write_blf(os.path.join(main_dir, "sample.blf"), frames)
    write_candump(os.path.join(main_dir, "sample_candump.log"), frames)
    write_csv(os.path.join(main_dir, "sample.csv"), frames)
    ch0 = [x for x in frames if x.channel == 0]
    write_trc(os.path.join(main_dir, "sample_channel1.trc"), ch0)
    per_ch = {}
    for x in frames:
        per_ch.setdefault(x.channel + 1, 0)
        per_ch[x.channel + 1] += 1
    nominal_load = sum(nominal_bits(x) for x in ch0) / (DURATION * 500_000)
    write_json(os.path.join(main_dir, "truth.json"), {
        "duration_s": DURATION, "total_frames": len(frames), "frames_per_channel": per_ch,
        "database": "../databases/a.dbc",
        "messages": [
            {"name": "EngineData", "id": 0x100, "cycle_ms": 10, "channels": [1, 2]},
            {"name": "VehicleSpeed", "id": 0x200, "cycle_ms": 20, "channels": [1, 3]},
            {"name": "BatteryStatus", "id": 0x300, "cycle_ms": 100, "channels": [1, 2]}],
        "channel1_nominal_bus_load_at_500kbit": round(nominal_load, 6),
        "note": "ASC, BLF, candump and CSV hold all three channels. The TRC holds channel 1 only. "
                "BLF is zlib compressed. Bus load ignores stuffing bits.",
        "counter_and_checksum": {"message": "VehicleSpeed", "counter": "bits 48-51 (+1 per frame, wraps at 16)",
                                 "checksum": "byte 7 = XOR of bytes 0 to 6"}})
    # ---- faults
    fdir = mkdir(out, "faults")
    ff = fault_sample(db)
    write_asc(os.path.join(fdir, "faults.asc"), ff)
    write_candump(os.path.join(fdir, "faults_candump.log"), ff)
    write_json(os.path.join(fdir, "truth.json"), {
        "database": "../databases/a.dbc", "faults": [
            {"type": "cycle_drift", "message": "EngineData", "from_s": 6.0,
             "detail": "period changes from 10 ms to 12 ms"},
            {"type": "stuck_value", "message": "EngineData", "signal": "EngineSpeed", "from_s": 7.0,
             "to_s": 8.0, "value": 1500.0},
            {"type": "spike", "message": "EngineData", "signal": "Throttle", "at_s": 3.0, "value": 100.0},
            {"type": "dropout", "message": "VehicleSpeed", "from_s": 4.0, "to_s": 4.5}]})

    # ---- CAN FD
    fdd = mkdir(out, "canfd")
    fd_frames, lengths = can_fd_sample()
    write_asc(os.path.join(fdd, "canfd.asc"), fd_frames)
    write_blf(os.path.join(fdd, "canfd.blf"), fd_frames)
    write_candump(os.path.join(fdd, "canfd_candump.log"), fd_frames)
    write_json(os.path.join(fdd, "truth.json"), {
        "frames": len(fd_frames), "payload_lengths": lengths,
        "fd_frames": sum(1 for x in fd_frames if x.is_fd),
        "brs_frames": sum(1 for x in fd_frames if x.bitrate_switch),
        "esi_frames": sum(1 for x in fd_frames if x.error_state_indicator),
        "extended_id_frames": sum(1 for x in fd_frames if x.is_extended_id),
        "frame_list": frames_to_jsonable(fd_frames[:50])})

    # ---- error frames
    edir = mkdir(out, "errors")
    ef, bursts = error_frame_sample(db)
    write_asc(os.path.join(edir, "errors.asc"), ef)
    write_blf(os.path.join(edir, "errors.blf"), ef)
    write_candump(os.path.join(edir, "errors_candump.log"), ef)
    write_json(os.path.join(edir, "truth.json"), {
        "error_frames": sum(n for _, n in bursts),
        "bursts": [{"start_s": t, "frames": n} for t, n in bursts],
        "normal_frames": sum(1 for x in ef if not x.is_error_frame)})

    # ---- J1939 transport
    jdir = mkdir(out, "j1939")
    jf, jt = j1939_sample()
    write_asc(os.path.join(jdir, "j1939_transport.asc"), jf)
    write_candump(os.path.join(jdir, "j1939_transport_candump.log"), jf)
    write_json(os.path.join(jdir, "truth.json"), {"transfers": jt, "total_frames": len(jf)})

    # ---- ISO-TP / UDS
    idir = mkdir(out, "isotp")
    itf, itt = isotp_sample()
    write_asc(os.path.join(idir, "isotp_uds.asc"), itf)
    write_candump(os.path.join(idir, "isotp_uds_candump.log"), itf)
    write_json(os.path.join(idir, "truth.json"), {"request_id": 0x7E0, "response_id": 0x7E8,
                                                  "transfers": itt, "total_frames": len(itf)})

    # ---- malformed files
    bad = mkdir(out, "malformed")
    make_malformed(bad, frames)

    make_extras(out, frames, db)

    if a.large:
        make_large(os.path.join(mkdir(out, "large"), "large_candump.log"), a.large, db)
    print("written to", out)


def make_malformed(bad, frames):
    base = frames[:200]
    expected = {}
    # truncated BLF: cut in the middle of an object
    tmp = os.path.join(bad, "_tmp.blf")
    write_blf(tmp, base)
    raw = open(tmp, "rb").read()
    os.remove(tmp)
    cut = int(len(raw) * 0.6)
    with open(os.path.join(bad, "truncated.blf"), "wb") as fh:
        fh.write(raw[:cut])
    expected["truncated.blf"] = {"problem": "file ends in the middle of an object", "valid_frames": "fewer than 200, none invalid"}

    # ASC with bad lines
    tmp = os.path.join(bad, "_tmp.asc")
    write_asc(tmp, base)
    lines = open(tmp).read().split("\n")
    os.remove(tmp)
    header_len = next(i for i, l in enumerate(lines) if l.strip() and l.strip()[0].isdigit())
    inserts = {
        header_len + 10: "   0.050000 1 100 Rx   d 8 ZZ 00 00 00 00 00 00 00",
        header_len + 40: "   garbage line without structure",
        header_len + 70: "   0.200000 1 100 Rx   d 9 00 00 00 00 00 00 00 00 00",
        header_len + 100: "   0.250000 1",
        header_len + 130: "   0.300000 1 100 Rx   d 8 00 11 22",
    }
    out_lines, bad_lines = [], []
    for i, l in enumerate(lines):
        if i in inserts:
            out_lines.append(inserts[i])
            bad_lines.append(len(out_lines))
        out_lines.append(l)
    with open(os.path.join(bad, "bad_lines.asc"), "w") as fh:
        fh.write("\n".join(out_lines))
    expected["bad_lines.asc"] = {"valid_frames": len(base), "bad_line_numbers": bad_lines,
                                 "problems": ["non-hex data", "no structure", "classic frame with DLC 9",
                                              "missing fields", "fewer data bytes than the length"]}

    # TRC with a damaged header
    tmp = os.path.join(bad, "_tmp.trc")
    write_trc(tmp, [x for x in base if x.channel == 0])
    txt = open(tmp).read().split("\n")
    os.remove(tmp)
    txt = [";$FILEVERSION=9.9" if l.startswith(";$FILEVERSION") else l for l in txt]
    txt = [l for l in txt if not l.startswith(";$COLUMNS")]
    with open(os.path.join(bad, "bad_header.trc"), "w") as fh:
        fh.write("\n".join(txt))
    expected["bad_header.trc"] = {"problem": "unknown file version and no column definition"}

    # CSV missing columns
    with open(os.path.join(bad, "missing_columns.csv"), "w") as fh:
        fh.write("timestamp,arbitration_id,data\n")
        for x in base[:50]:
            fh.write("%f,%x,%s\n" % (x.timestamp, x.arbitration_id, x.data.hex()))
        fh.write("0.5,100\n")
        fh.write("\n")
        fh.write("0.51\n")
    expected["missing_columns.csv"] = {"valid_frames": 50, "bad_line_numbers": [52, 54],
                                       "problem": "no length or channel column, and short rows"}

    # candump with garbage
    good = ["(%.6f) can0 %03X#%s" % (x.timestamp, x.arbitration_id, x.data.hex().upper()) for x in base[:60]]
    gl = list(good)
    gl.insert(10, "(0.050000) can0 12G#AABB")
    gl.insert(25, "(0.100000) can0 100 AABB")
    gl.insert(40, "(0.150000 can0 100#AABB")
    gl.insert(55, "not a frame")
    bad_texts = {"(0.050000) can0 12G#AABB", "(0.100000) can0 100 AABB", "(0.150000 can0 100#AABB",
                 "not a frame"}
    bad_numbers = [i + 1 for i, l in enumerate(gl) if l in bad_texts]
    with open(os.path.join(bad, "garbage.log"), "w") as fh:
        fh.write("\n".join(gl) + "\n")
    expected["garbage.log"] = {"valid_frames": 60, "bad_line_numbers": bad_numbers,
                               "problems": ["bad hex ID", "missing '#'", "missing ')'", "not a frame"]}

    # out-of-order timestamps
    oo = ["(%.6f) can0 %03X#%s" % (x.timestamp, x.arbitration_id, x.data.hex().upper()) for x in base[:40]]
    oo[20] = "(0.000500) can0 100#0000"
    oo[30] = "(0.001000) can0 100#0000"
    with open(os.path.join(bad, "out_of_order.log"), "w") as fh:
        fh.write("\n".join(oo) + "\n")
    expected["out_of_order.log"] = {"valid_frames": 40, "out_of_order_line_numbers": [21, 31]}

    open(os.path.join(bad, "empty.log"), "w").close()
    expected["empty.log"] = {"problem": "zero-byte file", "valid_frames": 0}
    with open(os.path.join(bad, "header_only.asc"), "w") as fh:
        fh.write("date Mon Jan 01 00:00:00.000 am 2026\nbase hex  timestamps absolute\ninternal events logged\n")
    expected["header_only.asc"] = {"problem": "header and no frames", "valid_frames": 0}
    with open(os.path.join(bad, "wrong_extension.dat"), "w") as fh:
        for x in base[:100]:
            fh.write("(%.6f) can0 %03X#%s\n" % (x.timestamp, x.arbitration_id, x.data.hex().upper()))
    expected["wrong_extension.dat"] = {"valid_frames": 100,
                                       "problem": "valid candump content under a misleading extension; the format must be detected from the content"}
    write_json(os.path.join(bad, "truth.json"), expected)


# ----------------------------------------------------------------------------- extra data
def crc8_j1850(data):
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def decode_frames(db, frames):
    rows = []
    for x in frames:
        try:
            msg = db.get_message_by_frame_id(x.arbitration_id)
        except KeyError:
            continue
        rows.append((x, msg.name, decode_phys(db, msg.name, x.data)))
    return rows


def segments(times, step, tol=1e-6):
    segs = []
    for t in times:
        if segs and t - segs[-1][1] <= step + tol:
            segs[-1][1] = t
        else:
            segs.append([t, t])
    return segs


def make_classic_truth(out, frames, db):
    d = os.path.join(out, "classic")
    ch1 = [f for f in frames if f.channel == 0]
    rows = decode_frames(db, ch1)
    with open(os.path.join(d, "truth_signals_channel1.csv"), "w") as fh:
        fh.write("time_s,id,message,signal,value\n")
        for x, name, vals in rows:
            for k, v in vals.items():
                fh.write("%.6f,0x%X,%s,%s,%.6f\n" % (x.timestamp, x.arbitration_id, name, k, float(v)))
    # plotting truth
    series = {}
    for x, name, vals in rows:
        for k, v in vals.items():
            series.setdefault(k, []).append((x.timestamp, float(v)))
    stats = {}
    for k, s in series.items():
        v = [y for _, y in s]
        stats[k] = {"count": len(v), "min": round(min(v), 6), "max": round(max(v), 6),
                    "mean": round(sum(v) / len(v), 6)}
    at = lambda sig, t: next(y for tt, y in series[sig] if abs(tt - t) < 1e-9)
    derived = [y - 2000 for _, y in series["EngineSpeed"]]
    write_json(os.path.join(d, "truth_plot.json"), {
        "channel": 1, "signal_stats": stats,
        "cursors": {"signal": "EngineSpeed", "t1": 2.0, "t2": 3.0, "value_t1": round(at("EngineSpeed", 2.0), 6),
                    "value_t2": round(at("EngineSpeed", 3.0), 6),
                    "difference": round(at("EngineSpeed", 3.0) - at("EngineSpeed", 2.0), 6)},
        "derived_signal": {"definition": "EngineSpeed - 2000", "min": round(min(derived), 6),
                           "max": round(max(derived), 6)},
        "note": "plot values are the decoded physical values; the same values are in truth_signals_channel1.csv"})
    # search and navigation truth
    ref = next(f for f in ch1 if f.arbitration_id == 0x300 and abs(f.timestamp - 5.0) < 1e-9)
    prefix = bytes(ref.data[:2])
    pat_hits = [f.timestamp for f in ch1 if f.arbitration_id == 0x300 and bytes(f.data[:2]) == prefix]
    hi = [t for t, y in series["EngineSpeed"] if y >= 2700.0]
    first5 = next(i for i, f in enumerate(frames) if f.timestamp >= 5.0)
    first5_ch1 = next(i for i, f in enumerate(ch1) if f.timestamp >= 5.0)
    cut_lo, cut_hi = 2.0, 3.0
    cut = {}
    for f in ch1:
        if cut_lo <= f.timestamp < cut_hi:
            cut["0x%X" % f.arbitration_id] = cut.get("0x%X" % f.arbitration_id, 0) + 1
    write_json(os.path.join(d, "truth_queries.json"), {
        "channel": 1,
        "by_id": {"id": 0x300, "count": sum(1 for f in ch1 if f.arbitration_id == 0x300)},
        "data_pattern": {"id": 0x300, "pattern": prefix.hex(" ").upper() + " ?? ?? ?? ?? ?? ??",
                         "matching_times": pat_hits},
        "signal_value": {"signal": "EngineSpeed", "condition": ">= 2700",
                         "matching_frames": len(hi), "segments": [[round(a, 6), round(b, 6)] for a, b in segments(hi, 0.010)]},
        "jump_to_time": {"time": 5.0, "first_frame_index_in_file": first5, "first_frame_index_in_channel1": first5_ch1,
                         "frame_id": frames[first5].arbitration_id},
        "cut": {"from_s": cut_lo, "to_s": cut_hi, "end_exclusive": True, "frames_by_id_channel1": cut,
                "frames_total_channel1": sum(cut.values())}})
    # script truth
    counts = {}
    for f in frames:
        counts.setdefault(str(f.channel), {})
        k = "0x%X" % f.arbitration_id
        counts[str(f.channel)][k] = counts[str(f.channel)].get(k, 0) + 1
    sdir = mkdir(out, "scripts")
    with open(os.path.join(sdir, "count_by_id.js"), "w") as fh:
        fh.write("""// Counts received frames per bus and ID and logs the totals on every tick.
var counts = {};

function setup ()
{
    host.log("count_by_id 1.0");
    host.setTickInterval(1000);
}

function gotCANFrame (bus, id, len, data)
{
    var key = bus + ":" + id.toString(16);
    counts[key] = (counts[key] || 0) + 1;
}

function tick ()
{
    for (var key in counts)
    {
        host.log("COUNT " + key + " " + counts[key]);
    }
}
""")
    write_json(os.path.join(sdir, "truth.json"), {
        "script": "count_by_id.js", "log": "../classic/sample.asc",
        "final_counts_by_bus_and_id": counts,
        "note": "bus 0 is channel 1 of the ASC file, bus 1 is channel 2, bus 2 is channel 3"})


def make_uncompressed_blf(src, dst):
    import struct, zlib
    raw = open(src, "rb").read()
    hdr_size = struct.unpack_from("<L", raw, 4)[0]
    out = bytearray(raw[:hdr_size])
    pos = hdr_size
    while pos + 16 <= len(raw):
        sig, hsz, hver, osz, otype = struct.unpack_from("<4sHHLL", raw, pos)
        assert sig == b"LOBJ", "bad object signature"
        body = raw[pos + hsz:pos + osz]
        if otype == 10:  # container
            method, = struct.unpack_from("<H", body, 0)
            usize, = struct.unpack_from("<L", body, 8)
            data = body[16:]
            if method == 2:
                data = zlib.decompress(data)
            assert len(data) == usize
            new_osz = 16 + 16 + len(data)
            out += struct.pack("<4sHHLL", b"LOBJ", 16, 1, new_osz, 10)
            out += struct.pack("<H6sLL", 0, b"\0" * 6, len(data), 0)
            out += data
            out += b"\0" * (new_osz % 4)
        else:
            out += raw[pos:pos + osz]
            out += b"\0" * (osz % 4)
        pos += osz + (osz % 4)
    struct.pack_into("<Q", out, 16, len(out))  # file size
    struct.pack_into("<Q", out, 24, len(out))  # uncompressed size
    open(dst, "wb").write(bytes(out))


def make_merge(out, db):
    d = mkdir(out, "merge")
    A, B = [], []
    for i in range(500):
        t = round(i * 0.010, 6)
        data, msg = encode(db, "EngineData", engine_values(t))
        A.append(m(t, msg.frame_id, data, ch=0))
    for j in range(250):
        tb = round(j * 0.020, 6)
        data, msg = encode(db, "VehicleSpeed", speed_values(tb + 0.25, j))
        B.append(m(tb, msg.frame_id, data, ch=1))
    sync = bytes([0xAA, 0x55, 0, 0, 0, 0, 0, 0])
    A.append(m(1.0, 0x7FF, sync, ch=0))
    B.append(m(0.75, 0x7FF, sync, ch=1))
    A.sort(key=lambda x: x.timestamp)
    B.sort(key=lambda x: x.timestamp)
    write_trc(os.path.join(d, "recorder_a.trc"), A)
    write_asc(os.path.join(d, "recorder_b.asc"), B)
    write_json(os.path.join(d, "truth.json"), {
        "recorder_a": {"file": "recorder_a.trc", "frames": len(A), "first_frame_time": 0.0},
        "recorder_b": {"file": "recorder_b.asc", "frames": len(B), "first_frame_time": 0.0},
        "clock_offset_s": 0.25,
        "note": "Both logs store time from their own first frame. Recorder B started 0.25 s after recorder A. "
                "Both recorders saw the same synchronisation frame (ID 0x7FF, data AA 55 00 00 00 00 00 00): at 1.0 s "
                "in A and at 0.75 s in B.",
        "merged": {"frames_with_both_sync_copies": len(A) + len(B), "frames_after_removing_duplicate_sync": len(A) + len(B) - 1,
                   "first_frame_of_b_in_merged_time": 0.25, "sync_time_in_merged_log": 1.0}})


def make_jitter(out, db):
    d = mkdir(out, "jitter")
    rng = random.Random(SEED + 1)
    t, frames = 0.0, []
    for i in range(1000):
        data, msg = encode(db, "EngineData", engine_values(t))
        frames.append(m(round(t, 6), msg.frame_id, data))
        t += max(0.005, rng.gauss(0.010, 0.0005))
    write_asc(os.path.join(d, "jitter.asc"), frames)
    write_candump(os.path.join(d, "jitter_candump.log"), frames)
    times = [f.timestamp for f in frames]
    iv = [b - a for a, b in zip(times, times[1:])]
    mean = sum(iv) / len(iv)
    std = (sum((x - mean) ** 2 for x in iv) / len(iv)) ** 0.5
    write_json(os.path.join(d, "truth.json"), {
        "message": "EngineData", "id": 0x100, "declared_cycle_ms": 10, "frames": len(frames),
        "interval_mean_s": round(mean, 6), "interval_std_s": round(std, 6),
        "interval_min_s": round(min(iv), 6), "interval_max_s": round(max(iv), 6),
        "note": "intervals computed from the stored (6-decimal) timestamps; population standard deviation"})


DBC_ERRORS = '''VERSION ""

NS_ :

BS_:

BU_: ECU_A

BO_ 256 EngineData: 8 ECU_A
 SG_ EngineSpeed : 0|16@1+ (0.25,0) [0|8000] "rpm" Vector__XXX
 SG_ Throttle : 8|8@1+ (0.4,0) [0|100] "%" Vector__XXX

BO_ 256 DuplicateId: 8 ECU_A
 SG_ DupSignal : 0|8@1+ (1,0) [0|255] "" Vector__XXX

BO_ 512 BadValues: 8 ECU_A
 SG_ ZeroScale : 0|8@1+ (0,0) [0|255] "" Vector__XXX
 SG_ Inverted : 8|8@1+ (1,0) [100|0] "" Vector__XXX
 SG_ TooLong : 56|16@1+ (1,0) [0|65535] "" Vector__XXX

BO_ 768 DuplicateSignal: 8 ECU_A
 SG_ Same : 0|8@1+ (1,0) [0|255] "" Vector__XXX
 SG_ Same : 8|8@1+ (1,0) [0|255] "" Vector__XXX

BO_ 1024 Valid: 8 ECU_A
 SG_ Level : 0|8@1+ (1,0) [0|255] "" Vector__XXX
'''


def make_dbc_errors(out):
    d = os.path.join(out, "databases")
    with open(os.path.join(d, "errors.dbc"), "w") as fh:
        fh.write(DBC_ERRORS)
    lines = DBC_ERRORS.split("\n")
    ln = lambda text, nth=1: [i + 1 for i, l in enumerate(lines) if text in l][nth - 1]
    write_json(os.path.join(d, "truth_dbc_errors.json"), {
        "file": "errors.dbc", "errors": [
            {"kind": "overlapping_signals", "message": "EngineData", "signals": ["EngineSpeed", "Throttle"], "line": ln("Throttle")},
            {"kind": "duplicate_message_id", "id": 256, "messages": ["EngineData", "DuplicateId"], "line": ln("BO_ 256", 2)},
            {"kind": "zero_scale", "signal": "ZeroScale", "line": ln("ZeroScale")},
            {"kind": "minimum_above_maximum", "signal": "Inverted", "line": ln("Inverted")},
            {"kind": "signal_beyond_message_length", "signal": "TooLong", "line": ln("TooLong")},
            {"kind": "duplicate_signal_name", "message": "DuplicateSignal", "signal": "Same", "line": ln("Same", 2)}],
        "valid_message": "Valid", "total_errors": 6})


SYM_PCAN = '''FormatVersion=6.0 // Do not edit this line!
Title="Synthetic PCAN-style symbols"

{ENUMS}
Enum=GearEnum(0="Park", 1="Reverse", 2="Neutral", 3="Drive")

{SENDRECEIVE}

[EngineData]
ID=100h
Len=8
CycleTime=10
Var=EngineSpeed unsigned 0,16 /u:rpm /f:0.25 /min:0 /max:8000
Var=Throttle unsigned 16,8 /u:% /f:0.4 /min:0 /max:100
Var=CoolantTemp unsigned 24,8 /u:degC /o:-40 /min:-40 /max:215

[Transmission]
ID=1A0h
Len=8
CycleTime=20
Var=Gear unsigned 0,3 /e:GearEnum
Var=OutputTorque signed 8,16 /u:Nm /f:0.5

[J1939Status]
ID=18FEF100h
Type=Extended
Len=8
Var=Pressure unsigned 0,16 /u:kPa /f:0.5
'''


def make_sym_pcan(out):
    d = os.path.join(out, "databases")
    text = SYM_PCAN
    with open(os.path.join(d, "pcan_style.sym"), "w") as fh:
        fh.write(text)
    db = cantools.database.load_string(text, database_format="sym")
    write_json(os.path.join(d, "truth_pcan_style.json"), {
        "file": "pcan_style.sym", "style": "hand-written in the PCAN Symbol Editor text style, not produced by a library",
        "messages": [{"name": mm.name, "id": mm.frame_id, "extended": mm.is_extended_frame,
                      "signals": [s.name for s in mm.signals]} for mm in db.messages],
        "enum": {"GearEnum": {"0": "Park", "1": "Reverse", "2": "Neutral", "3": "Drive"}}})


def make_reverse(out):
    d = mkdir(out, "reverse")
    rdb = Database(messages=[
        Message(0x210, "Powertrain2", 8, [
            sig("Torque", 4, 12, 0.5, -500, -500, 1500, "Nm"),
            sig("Gear", 16, 3, 1, 0, 0, 7),
            sig("Counter", 20, 4, 1, 0, 0, 15),
            sig("Flag", 24, 1, 1, 0, 0, 1),
            sig("Temp", 32, 8, 1, -40, -40, 215, "degC"),
            sig("Crc", 56, 8, 1, 0, 0, 255)], cycle_time=20),
        Message(0x220, "Hydraulics", 8, [
            sig("Pressure", 7, 16, 0.1, 0, 0, 6000, "kPa", byte_order="big_endian"),
            sig("Temp", 16, 8, 1, -40, -40, 215, "degC"),
            sig("Counter", 52, 4, 1, 0, 0, 15),
            sig("Sum", 56, 8, 1, 0, 0, 255)], cycle_time=20)])
    rng = random.Random(SEED + 2)
    frames = []
    for i in range(500):
        t = round(i * 0.020, 6)
        gear = 1 + min(3, int(t // 2.5))
        v = {"Torque": 100 + 80 * math.sin(2 * math.pi * t / 6), "Gear": gear, "Counter": i % 16,
             "Flag": (i // 25) % 2, "Temp": 70 + 10 * math.sin(2 * math.pi * t / 30), "Crc": 0}
        data = bytearray(rdb.get_message_by_name("Powertrain2").encode(v, scaling=True, strict=False))
        data[7] = crc8_j1850(bytes(data[:7]))
        frames.append(m(t, 0x210, bytes(data)))
        v = {"Pressure": 250 + 100 * math.sin(2 * math.pi * t / 4), "Temp": 60 + 5 * math.sin(2 * math.pi * t / 20),
             "Counter": i % 16, "Sum": 0}
        data = bytearray(rdb.get_message_by_name("Hydraulics").encode(v, scaling=True, strict=False))
        data[7] = sum(data[:7]) & 0xFF
        frames.append(m(round(t + 0.001, 6), 0x220, bytes(data)))
        noise = rng.randrange(256)
        frames.append(m(round(t + 0.002, 6), 0x230, bytes([0xA5, 0x00, noise, (i // 50) % 2, 0, 0, 0, 0])))
    frames.sort(key=lambda x: x.timestamp)
    write_asc(os.path.join(d, "reverse.asc"), frames)
    write_candump(os.path.join(d, "reverse_candump.log"), frames)
    cantools.database.dump_file(rdb, os.path.join(d, "reverse_answer.dbc"))
    write_json(os.path.join(d, "truth.json"), {
        "frames": len(frames), "cycle_ms": 20,
        "answer_database": "reverse_answer.dbc (0x230 is not in it)",
        "messages": {
            "0x210": {"name": "Powertrain2", "byte_order": "little_endian", "signals": [
                {"name": "Torque", "start_bit": 4, "length": 12, "scale": 0.5, "offset": -500, "note": "unaligned"},
                {"name": "Gear", "start_bit": 16, "length": 3, "values": [1, 2, 3, 4], "note": "steps at 2.5, 5.0 and 7.5 s"},
                {"name": "Counter", "start_bit": 20, "length": 4, "note": "+1 per frame, wraps at 16"},
                {"name": "Flag", "start_bit": 24, "length": 1, "note": "toggles every 25 frames"},
                {"name": "Temp", "start_bit": 32, "length": 8, "scale": 1, "offset": -40}],
                "checksum": {"byte": 7, "algorithm": "CRC-8 SAE J1850 (polynomial 0x1D, initial 0xFF, final xor 0xFF) over bytes 0 to 6"}},
            "0x220": {"name": "Hydraulics", "signals": [
                {"name": "Pressure", "byte_order": "big_endian", "start_bit_msb": 7, "length": 16, "scale": 0.1},
                {"name": "Temp", "start_bit": 16, "length": 8, "scale": 1, "offset": -40},
                {"name": "Counter", "start_bit": 52, "length": 4, "note": "high nibble of byte 6, +1 per frame"}],
                "checksum": {"byte": 7, "algorithm": "sum of bytes 0 to 6 modulo 256"}},
            "0x230": {"name": "unknown", "constant_bytes": {"0": "0xA5", "1": "0x00", "4-7": "0x00"},
                      "noise_byte": 2, "slow_toggle_byte": {"byte": 3, "period_frames": 50},
                      "checksum": None}}})


def make_support_files(out, frames, db):
    ff = fault_sample(db)
    rows = decode_frames(db, ff)
    rdir = mkdir(out, "rules")
    rules = [
        {"name": "high_rpm", "when": "EngineSpeed > 2700", "bus": 1, "action": "mark"},
        {"name": "speed_missing", "when": "no frame with ID 0x200 for 100 ms", "bus": 1, "action": "alert"},
        {"name": "throttle_spike", "when": "Throttle >= 99", "bus": 1, "action": "start_recording",
         "duration_s": 2},
    ]
    write_json(os.path.join(rdir, "rules.json"), {"log": "../faults/faults.asc", "database": "../databases/a.dbc",
                                                    "note": "illustrative rule format; rename fields freely", "rules": rules})
    hi = [x.timestamp for x, n, v in rows if n == "EngineData" and v["EngineSpeed"] > 2700]
    sp = [x.timestamp for x in ff if x.arbitration_id == 0x200]
    gaps = [(a, b) for a, b in zip(sp, sp[1:]) if b - a > 0.100 + 1e-9]
    spike = [x.timestamp for x, n, v in rows if n == "EngineData" and v["Throttle"] >= 99]
    write_json(os.path.join(rdir, "truth.json"), {
        "high_rpm": {"matching_frames": len(hi), "segments": [[round(a, 6), round(b, 6)] for a, b in segments(hi, 0.0121)]},
        "speed_missing": {"alerts": [{"alert_time": round(a + 0.100, 6), "last_seen": a, "next_seen": b} for a, b in gaps]},
        "throttle_spike": {"trigger_times": spike, "recording_from": spike[0] if spike else None,
                           "recording_to": round(spike[0] + 2, 6) if spike else None}})
    tdir = mkdir(out, "transmit")
    items = [
        {"name": "EngineData", "id": "0x100", "cycle_ms": 10,
         "signals": {"EngineSpeed": 2000.0, "Throttle": 20.0, "CoolantTemp": 85.0}},
        {"name": "BatteryStatus", "id": "0x300", "cycle_ms": 100,
         "signals": {"Voltage": 380.0, "Current": 12.5, "StateOfCharge": 80.0}},
        {"name": "OneShot", "id": "0x7DF", "send": "once", "data": "02 01 0C 00 00 00 00 00"}]
    write_json(os.path.join(tdir, "messages.json"), {"bus": 1, "database": "../databases/a.dbc",
                                                       "note": "illustrative list format; rename fields freely",
                                                       "messages": items})
    enc = {}
    for it in items[:2]:
        data, _ = encode(db, it["name"], it["signals"])
        enc[it["name"]] = data.hex(" ").upper()
    write_json(os.path.join(tdir, "truth.json"), {
        "run_seconds": 2, "expected_frames": {"EngineData": 200, "BatteryStatus": 20, "OneShot": 1},
        "timing_tolerance": "each cycle within 1 ms of its stated period on the virtual bus",
        "encoded_data": enc})


def make_extras(out, frames, db):
    make_classic_truth(out, frames, db)
    make_uncompressed_blf(os.path.join(out, "classic", "sample.blf"),
                          os.path.join(out, "classic", "sample_uncompressed.blf"))
    make_merge(out, db)
    make_jitter(out, db)
    make_dbc_errors(out)
    make_sym_pcan(out)
    make_reverse(out)
    make_support_files(out, frames, db)


def make_large(path, n, db):
    rng = random.Random(SEED)
    ids = [0x100 + 0x10 * i for i in range(40)]
    t = 0.0
    with open(path, "w") as fh:
        for i in range(n):
            t += 0.0001
            arb = ids[i % len(ids)]
            data = "".join("%02X" % rng.randrange(256) for _ in range(8))
            fh.write("(%.6f) vcan0 %03X#%s\n" % (t, arb, data))


if __name__ == "__main__":
    main()
