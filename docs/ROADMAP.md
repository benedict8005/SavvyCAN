# Roadmap

This fork extends [SavvyCAN](https://github.com/collin80/SavvyCAN) into a simpler analyser that does the everyday jobs of a CAN analyser well. You should be able to open a log in any common format or connect to a live bus, read the traffic as decoded signals, check how healthy the bus is, investigate unknown messages and replay traffic, without reading a manual.

Work is tracked in issues, grouped by milestone. Each issue is one pull request.

## Workload 1: Read and understand traffic
Read recorded and live traffic (classic CAN and CAN FD) into one form and show it as raw frames, decoded signals and complete messages.
- Test harness and synthetic data
- Log reading
- Frame search and bookmarks
- J1939 transport and diagnostics
- Live and replay plotting
- Hardening and regression tests

## Workload 2: Check and investigate
Tell whether a bus is healthy, record and be alerted while it runs, and understand unknown traffic.
- Bus statistics and health report
- Signal anomaly detection
- Live monitoring and continuous recording
- Reverse-engineering assistant
- Log merging and cutting
- Signal export and reports

## Workload 3: Use, automate and release
Make the application pleasant to use and easy to install.
- Transmit and replay
- Simpler user interface
- Scripting and command line
- Packaging, documentation and release
- Projects and sessions

## Workload 4: Signal databases
- Symbol (.sym) file support
- Several databases at once
- Database validation
- Signal browser and change report
- Database merge

## How it is tested
Every feature is tested without hardware, using generated logs with known contents (`test/data/synthetic`) and Qt's virtual CAN bus. The whole suite runs headless with `make check`.
