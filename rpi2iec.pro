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
}

# To compile for Raspberry PI, run qmake with the flags: CONFIG+=raspberry
raspberry {
		# So wiringPi include files can be found during compile
		INCLUDEPATH += /usr/local/include
		# To link the wiringPi library when making the executable
		LIBS += -L/usr/local/lib -lwiringPi
		# To conditionally compile wiringPi so that it still builds on other platforms.
		DEFINES += "HAS_WIRINGPI="
} #raspberry

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
		x64driver.cpp

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
		x64driver.hpp

FORMS += mainwindow.ui \
		aboutdialog.ui \
		settingsdialog.ui

OTHER_FILES += \
		changes.txt \
		notes.txt \
		README.TXT \
		rpi2iec.rc

RESOURCES += \
		resources.qrc


CONFIG(debug, debug|release) {
	DESTDIR = debug
	OBJECTS_DIR = debug/.obj
	MOC_DIR = debug/.moc
	RCC_DIR = debug/.rcc
	UI_DIR = debug/.ui
} else {
	DESTDIR = release
	OBJECTS_DIR = release/.obj
	MOC_DIR = release/.moc
	RCC_DIR = release/.rcc
	UI_DIR = release/.ui
}

