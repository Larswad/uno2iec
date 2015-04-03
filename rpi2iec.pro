#-------------------------------------------------
#
# Project created by QtCreator 2013-04-17T11:25:40
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = rpi2iec
TEMPLATE = app

DEFINES += CONSOLE_DEBUG

win32-msvc* {
	# Multiple build processes with jom
	# only works in .pro file for MSVC compilers, for gnu add -j8 in projects / make
	QMAKE_CXXFLAGS += /MP
}
else {
	# explicit enabling of c++11 under all gnu compilers.
	QMAKE_CXXFLAGS += -std=gnu++0x
}

win32 {
	# version resource and appicon.
	RC_FILE = rpi2iec.rc
	OBJPRE = win
	# Add this for static linking of mingw libs. Note: Needs static Qt version.
	# QMAKE_LFLAGS += -static-libgcc -static-libstdc++ -static
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
				qcmdtextedit.cpp \
				mountspecificfile.cpp

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
				qcmdtextedit.h \
				mountspecificfile.h \
				utils.hpp

FORMS += mainwindow.ui \
				aboutdialog.ui \
				settingsdialog.ui \
				logfiltersetup.ui \
				mountspecificfile.ui

OTHER_FILES += \
				changes.txt \
				notes.txt \
				README.TXT \
				rpi2iec.rc \
				icons/ok.png \
				icons/arrow_refresh.png \
				icons/browse.png \
				icons/cancel.png \
				icons/clear.png \
				icons/exit.png \
				icons/filter.png \
				icons/floppy_mount.png \
				icons/floppy_unmount.png \
				icons/html.png \
				icons/pause.png \
				icons/restart.png \
				icons/save.png \
				icons/settings.png \
				icons/theme.png \
				icons/1541.ico \
				other/dos1541 \
				fonts/PetMe2X.ttf \
				fonts/PetMe64.ttf \
				fonts/PetMe1282Y.ttf

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
