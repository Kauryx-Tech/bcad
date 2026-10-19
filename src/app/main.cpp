#include "MainWindow.h"
#include "UiScale.h"
#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // MSAA : le tracé CAO est nettement plus lisible avec l'anticrénelage
    QSurfaceFormat::setDefaultFormat(format);

    // Echelle de l'interface (celle de Windows sous WSL), avant QApplication.
    bcad::app::applyUiScale();
    QApplication app(argc, argv);
    QApplication::setApplicationName("bcad");
    QApplication::setOrganizationName("bcad");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/bcad-logo.svg")));

    bcad::app::MainWindow window;
    window.show();

    return app.exec();
}
