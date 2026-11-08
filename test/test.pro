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
    ../connections/canbus.cpp


#HEADERS += \
#    ../utils/lfqueue.h

target.path= .
INSTALLS += target

HEADERS += \
    tst_lfqueue.h \
    tst_blf.h \
    tst_ascheader.h \
    tst_nativecsv.h \
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
