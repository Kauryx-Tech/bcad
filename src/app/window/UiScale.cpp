#include "window/UiScale.h"

#include <QSettings>
#include <QtGlobal>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace bcad::app {

namespace {

bool runningUnderWsl() {
    if (std::getenv("WSL_DISTRO_NAME")) return true;
    std::ifstream version("/proc/version");
    std::string line;
    std::getline(version, line);
    return line.find("microsoft") != std::string::npos || line.find("Microsoft") != std::string::npos;
}

// Echelle d'affichage de Windows (AppliedDPI / 96) lue par reg.exe, sans
// reseau ni dependance. 1 si la lecture echoue.
double windowsDisplayScale() {
    FILE* pipe = popen(
        "timeout 3 reg.exe query 'HKCU\\Control Panel\\Desktop\\WindowMetrics' /v AppliedDPI 2>/dev/null", "r");
    if (!pipe) return 1.0;
    char buffer[256];
    std::string output;
    while (std::fgets(buffer, sizeof buffer, pipe)) output += buffer;
    pclose(pipe);
    const auto at = output.find("0x");
    if (at == std::string::npos) return 1.0;
    const long dpi = std::strtol(output.c_str() + at, nullptr, 16);
    return (dpi >= 96 && dpi <= 480) ? dpi / 96.0 : 1.0;
}

} // namespace

double resolveUiScale() {
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) return 0.0;
    // QSettings explicite : QApplication n'existe pas encore.
    QSettings settings(QStringLiteral("bcad"), QStringLiteral("bcad"));
    const double chosen = settings.value(QString::fromLatin1(kUiScaleSetting), 0.0).toDouble();
    if (chosen >= 0.75 && chosen <= 3.0) return chosen;
    return runningUnderWsl() ? windowsDisplayScale() : 1.0;
}

void applyUiScale() {
    const double scale = resolveUiScale();
    if (scale > 0.0 && std::abs(scale - 1.0) > 0.01) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scale, 'f', 2));
        std::fprintf(stderr, "bcad[ui]: echelle de l'interface %.0f %%\n", scale * 100.0);
    }
}

} // namespace bcad::app
