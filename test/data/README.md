# Synthetic test data

Every file under `synthetic/` is invented. Nothing comes from a real vehicle, person or company.
Each folder has a `truth.json` (or similar truth file) that states what the files contain, so a test
can compare a reader's output with a known answer.

Regenerate with `python3 test/data/make_synthetic_data.py` (needs python-can and cantools).
Add `--large 2000000` to also write a large candump log (about 79 MB, not stored in the repository).
The frames and truth files are deterministic. Some file headers record the time of generation.

The `rules.json`, `messages.json` and `count_by_id.js` files use illustrative formats. Rename fields
freely. Log formats store times relative to the first frame, so the main files start with a frame
at 0 s. The files were read back with python-can and cantools, not with SavvyCAN's own readers.

`test/tst_ingest.cpp` loads the ASC, BLF, TRC, CSV and candump files of this set through SavvyCAN's log
ingestion layer (`ingest/`) and compares the result with the truth files: frame counts per channel,
CAN FD flags, payloads, error-frame bursts, the line numbers of malformed records and the recovery from
the damaged files in `malformed/`. `test/tst_stats.cpp` runs the statistics engine (`stats/`) over the
`classic/` (bus load), `jitter/` (interval statistics), `faults/` (dropout, cycle drift, stuck signal value
and spike), `errors/` (error bursts) and `canfd/` sets and checks the figures against the truth files. `test/tst_layout.cpp` runs the
message layout analysis (`re/`) over `classic/`: it must find the 4-bit counter and the XOR checksum of
`VehicleSpeed` and the 16-bit Intel values of the other messages. `test/tst_logedit.cpp` merges the `merge/`
recorders (the 0.25 s clock offset must come out of the shared sync frame) and cuts `classic/` by the time,
id, data pattern and signal queries of `truth_queries.json`. `test/tst_export.cpp` exports the signals of
`classic/` through `databases/a.dbc` as a CSV time series (`export/`) and checks every channel-1 value against
`truth_signals_channel1.csv`.

## Folders

- `classic/` Ten seconds of classic CAN on three channels (1,600 frames on channel 1, 1,100 on
  channel 2, 500 on channel 3) in ASC, BLF (compressed and uncompressed), candump and CSV, plus a
  TRC of channel 1. Channels 1 and 2 reuse IDs 0x100 and 0x300 with different values. Message 0x200
  has a 4-bit counter and an XOR checksum. Truth files: decoded values of every channel-1 frame
  (`truth_signals_channel1.csv`), plotting figures such as cursor values and statistics
  (`truth_plot.json`), and answers to search, jump-to-time and cut queries (`truth_queries.json`).
- `faults/` One channel with planted faults at known times: cycle drift, a stuck value, a spike and
  a dropout.
- `jitter/` 1,000 frames of one message with random timing jitter, and the exact mean and spread of
  the intervals.
- `canfd/` 400 frames mixing classic and CAN FD, payloads from 8 to 64 bytes, with bit-rate switch,
  error-state flags and extended IDs, in ASC, BLF and candump.
- `errors/` Normal traffic with three bursts of error frames (3, 1 and 10 frames) in ASC, BLF and
  candump.
- `j1939/` Single-frame traffic plus six J1939 transport transfers (broadcast and connection-based)
  carrying active-fault messages: three complete, one with a missing packet, one with packets out
  of order, and one aborted request.
- `isotp/` Five ISO-TP and UDS exchanges: a multi-frame response, a negative response, a
  100-byte response, one with a missing consecutive frame and one with a wrong sequence number.
- `reverse/` Three messages for working out undocumented layouts: unaligned and big-endian signals,
  a counter, a CRC-8 checksum, a sum checksum, and a message with constant bytes and a noise byte.
  `reverse_answer.dbc` holds the correct layout of two of them.
- `merge/` Two recorder logs (TRC and ASC) whose clocks differ by 0.25 s, with a shared
  synchronisation frame.
- `databases/` `a.dbc` and the same database as `a.sym`; `a_changed.dbc` with seven known changes
  (`truth_dbc_diff.json`); `b.sym`, which clashes with `a.dbc` on ID 0x100 and adds 0x600
  (`truth_clashes.json`); `errors.dbc` with six planted errors and their line numbers
  (`truth_dbc_errors.json`); and `pcan_style.sym`, a hand-written symbol file in the PCAN style with
  an enum and an extended ID.
- `rules/` Watch rules for the fault log and the alerts and marks they should produce.
- `transmit/` A transmit list and the number and content of frames it should send in two seconds.
- `scripts/` A scripting-interface script that counts frames per bus and ID, and the expected totals.
- `malformed/` Damaged files with the expected problems and line numbers in `truth.json`: a
  truncated BLF, a bad TRC header, an ASC with bad lines, a CSV with missing columns, a candump with
  garbage, out-of-order timestamps, an empty file, a header-only file, and valid candump content
  under a `.dat` extension.

The existing real-capture samples are in `examples/`.

## Live traffic without hardware

`test/tools/virtualcan_traffic.py` feeds live frames into SavvyCAN through Qt's `virtualcan` bus
(Connection window -> QT SerialBus Devices -> `virtualcan` / `can0`). It sends the messages of
`synthetic/databases/a.dbc` with sine-wave signals at a configurable rate, plus an extended-ID frame
and an OBD-style response, so the graphing window, the search window and the protocol decoders can be
exercised against a real connection and against the Playback window replaying onto that bus.
