#include "bcad/app/MainWindow.h"
#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // MSAA : le tracé CAO est nettement plus lisible avec l'anticrénelage
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName("bcad");
    QApplication::setOrganizationName("bcad");

    bcad::app::MainWindow window;
    window.show();

    return app.exec();
}
