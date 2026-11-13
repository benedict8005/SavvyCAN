# Tests

There are two kinds of test project here.

## The main test program (`test.pro`)

Small, fast and headless. It links only the file readers and helpers it needs, so it builds in under a minute. `main.cpp` runs every class in turn and exits non-zero if any fail.

```sh
mkdir -p ~/build && cd ~/build
qmake <repo>/test/test.pro && make -j8
QT_QPA_PLATFORM=offscreen ./test      # or: make check
```

Currently included: lock-free queue, BLF, ASC header, native CSV, the synthetic classic ASC check, and the virtual bus test. New tests belong here, and in `main.cpp`, unless they need the main window.

## Full-application projects

`cabana`, `capturepause`, `framesender`, `isotpexport`, `multiplexroundtrip`, `originalplayback` and `socketcand` each build the whole application source set with a test entry point, because they drive real windows and models. They take several minutes to compile, so they stay as separate projects and are not part of `make check`. Each has its own `README-<name>.md` with the build and run commands, and all run with `QT_QPA_PLATFORM=offscreen`.

The old `tst_cancon` hardware test was removed. It needed a real adapter and a constructor with arguments, so it could not run in `main.cpp`. `tst_virtualbus.h` covers sending and receiving on Qt's virtual CAN bus instead, and skips when the plugin is missing.

## Test data

`data/make_synthetic_data.py` writes the logs and `truth.json` files in `data/synthetic`; see `data/README.md`. Work expected numbers out from the truth files, not from the reader's own output.
