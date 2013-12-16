#-------------------------------------------------
#
# Project created by QtCreator 2013-04-17T11:25:40
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = rpi2iec
TEMPLATE = app

DEFINES += CONSOLE_DEBUG

#CONFIG += extserialport
#LIBS += -L../debug/qextserialport -lqextserialport1
#LIBS += -L../debug/qextserialport -lQt5ExtSerialPortd1
INCLUDEPATH += ../qextserialport/src
#QMAKE_LIBDIR += ../qextserialport/src/build
include(../qextserialport/src/qextserialport.pri)

QMAKE_CXXFLAGS += -std=gnu++0x
    # QMAKE_CXXFLAGS +=-std=c++0x


win32 {
    # version resource and appicon.
    RC_FILE = rpi2iec.rc
    OBJPRE = win
}

unix {
    OBJPRE = nix
}

# To compile for Raspberry PI, run qmake with the flags: CONFIG+=raspberry
raspberry {
    # So wiringPi include files can be found during compile
    INCLUDEPATH += /usr/local/include
    # To link the wiringPi library when making the executable
    LIBS += -L/usr/local/lib -lwiringPi
    # To conditionally compile wiringPi so that it still builds on other platforms.
    DEFINES += "HAS_WIRINGPI="

    OBJPRE = pi
} #raspberry

mac {
  OBJPRE = mac
}

SOURCES += main.cpp\
        mainwindow.cpp \
        t64driver.cpp \
        m2idriver.cpp \
        d64driver.cpp \
        filedriverbase.cpp \
        interface.cpp \
        nativefs.cpp \
        logger.cpp \
        x00fs.cpp \
        aboutdialog.cpp \
        settingsdialog.cpp \
        doscommands.cpp \
        x64driver.cpp \
    logfiltersetup.cpp \
    qcmdtextedit.cpp

HEADERS += mainwindow.hpp \
        t64driver.hpp \
        m2idriver.hpp \
        d64driver.hpp \
        filedriverbase.hpp \
        interface.hpp \
        nativefs.hpp \
        logger.hpp \
        x00fs.hpp \
        version.h \
        aboutdialog.hpp \
        settingsdialog.hpp \
        dirlistthemingconsts.hpp \
        doscommands.hpp \
        uno2iec/cbmdefines.h \
        x64driver.hpp \
    logfiltersetup.hpp \
    qcmdtextedit.h

FORMS += mainwindow.ui \
        aboutdialog.ui \
        settingsdialog.ui \
    logfiltersetup.ui

OTHER_FILES += \
        changes.txt \
        notes.txt \
        README.TXT \
        rpi2iec.rc

RESOURCES += \
        resources.qrc



# we want intermediate build files stored in configuration and platform specific folders.
# Reason is not getting compile errors when switching from building under one platform to another.
# Object file formats are different. We don't want to mix release and debug either.
CONFIG(debug, debug|release) {
    REL = debug
} else {
    REL = release
}

OBJECTS_DIR = $$quote($${REL}/.obj$${OBJPRE})
DESTDIR = $$quote($${REL})
MOC_DIR = $$quote($${REL}/.moc)
RCC_DIR = $$quote($${REL}/.rcc)
UI_DIR = $$quote($${REL}/.ui)
