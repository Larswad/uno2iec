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

#INCLUDES += <path to wiring pi>

#LIBS += -lwiringPi
#alternative:
#TARGETDEPS += libwiringpi.a

# QMAKE_CXXFLAGS +=-std=c++0x

SOURCES += main.cpp\
				mainwindow.cpp \
		t64driver.cpp \
		m2idriver.cpp \
		d64driver.cpp \
		filedriverbase.cpp \
		interface.cpp \
		nativefs.cpp

HEADERS  += mainwindow.hpp \
		t64driver.hpp \
		m2idriver.hpp \
		d64driver.hpp \
		filedriverbase.hpp \
		interface.hpp \
		nativefs.hpp

FORMS    += mainwindow.ui

OTHER_FILES += \
		readme.txt \
		changes.txt \
		notes.txt
