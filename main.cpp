//
// Title        : rpi2iec - main
// Author       : Lars Wadefalk
// Date         : August 2013.
//
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//


#include "mainwindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QFontDatabase>
#include <QStyleFactory>

#ifdef CONSOLE_DEBUG
#include <QDebug>
#endif

bool addEmbeddedFonts();


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef CONSOLE_DEBUG
    // Say hi
    qDebug() << endl << "Welcome." << endl;
#endif

    foreach(QString style, QStyleFactory::keys())
        qDebug() << "style: " << style;

    if(QStyleFactory::keys().contains("WindowsXP"))
        a.setStyle(QStyleFactory::create("WindowsXP"));
    else
        a.setStyle(QStyleFactory::create("Fusion"));

    a.setOrganizationName("MumboJumbo_Software"/*VER_COMPANYNAME_STR*/);
    a.setOrganizationDomain("DOMAIN");
    a.setApplicationName("Uno2IEC");

    addEmbeddedFonts();

    MainWindow w;
    w.show();
    w.checkVersion();

    return a.exec();
} // main


// Add embedded (truetype) fonts located in resources to application font database.
// This works even if the font is not registered in system globally.
bool addEmbeddedFonts()
{
    QStringList list({ "PetMe64.ttf" , "PetMe2X.ttf" , "PetMe1282Y.ttf" });
    int fontID(-1);
    bool fontWarningShown(false), success(true);

    foreach(const QString& strFont, list) {
        QFile res(":/fonts/fonts/" + strFont);
        if(!res.open(QIODevice::ReadOnly)) {
            success = false;
        }
        else {
            fontID = QFontDatabase::addApplicationFontFromData(res.readAll());
            if(-1 == fontID)
                success = false;
        }

        // It is enough to display warning only once.
        if(!success and !fontWarningShown) {
            QMessageBox::warning(0, "Application", QString("Can't open the font %1 %2 %3.").arg(QChar(0x00AB)).arg(strFont).arg(QChar(0x00BB)));
            fontWarningShown = true;
        }
    }
    return success;
} // addEmbeddedFonts
