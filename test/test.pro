QT += core gui serialbus widgets testlib serialbus


CONFIG += c++17

INCLUDEPATH += ../ ../connections

SOURCES += \
    tst_lfqueue.cpp \
    tst_blf.cpp \
    main.cpp \
    ../blfhandler.cpp \
    ../framefileio.cpp \
    ../pcaplite.cpp \
    ../utility.cpp \
    ../can_structs.cpp \
    ../connections/canbus.cpp \
    ../ingest/logformat.cpp \
    ../ingest/candumpparser.cpp \
    ../ingest/ascparser.cpp \
    ../ingest/trcparser.cpp \
    ../ingest/csvparser.cpp \
    ../ingest/gvretparser.cpp \
    ../ingest/logreader.cpp


#HEADERS += \
#    ../utils/lfqueue.h

target.path= .
INSTALLS += target

HEADERS += \
    tst_lfqueue.h \
    tst_blf.h \
    tst_ascheader.h \
    tst_nativecsv.h \
    tst_synthetic_classic.h \
    tst_virtualbus.h \
    tst_logformat.h \
    tst_candumpparser.h \
    tst_ascparser.h \
    tst_trcparser.h \
    tst_csvparser.h \
    tst_gvretparser.h \
    tst_legacybus.h \
    tst_logreader.h \
    tst_largelog.h \
    ../ingest/logformat.h \
    ../ingest/candumpparser.h \
    ../ingest/ascparser.h \
    ../ingest/trcparser.h \
    ../ingest/csvparser.h \
    ../ingest/gvretparser.h \
    ../ingest/logreader.h \
    ../blfhandler.h \
    ../framefileio.h \
    ../pcaplite.h \
    ../utility.h \
    ../can_structs.h \
    ../connections/canbus.h

# Run the whole suite headless: make check
check.commands = QT_QPA_PLATFORM=offscreen ./$(TARGET)
check.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += check
