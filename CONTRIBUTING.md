# Contributing

## Build and test

Requires Qt 5.15 with the serialbus, serialport, qml and help modules. On Debian or Ubuntu:

    sudo apt install qt5-qmake qtbase5-dev libqt5serialbus5-dev libqt5serialport5-dev \
        qtdeclarative5-dev qttools5-dev libqt5opengl5-dev

Build and run the tests (no display needed):

    mkdir build && cd build
    qmake ../test/test.pro && make -j8
    QT_QPA_PLATFORM=offscreen ./test

Build the application with `qmake SavvyCAN.pro && make`.

## Tests

- New behaviour comes with a test in `test/`, added to `test/test.pro` and `test/main.cpp`.
- Tests use the generated data in `test/data/synthetic` and Qt's virtual CAN bus, never real hardware. Regenerate the data with `python3 test/data/make_synthetic_data.py`. It needs pip install python-can cantools.
- Check results against the truth files next to the data rather than numbers worked out by hand.
- QCanBusFrame cannot hold ids above 0x1FFFFFFF, so do not test SocketCAN flag bits through it.

## Continuous integration

GitHub Actions is switched off on this fork. The workflow inherited from upstream publishes releases and posts to Discord, so it should not run here. A small workflow that builds and runs the headless tests will be added later.

## Branches and pull requests

- One issue is one pull request, on a branch named `roadmap/<issue-number>-<short-name>`.
- Open pull requests against `master`. Use "Refs #N" while the issue is in progress and "Closes #N" when its "Done when" line is met.
- Keep commits small and each one passing the full suite.

## Commit messages

Imperative subject under 72 characters, for example `Add candump reader for CAN FD`. Add a short body only when the reason is not obvious.
