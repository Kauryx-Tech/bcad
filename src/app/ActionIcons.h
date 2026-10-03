#pragma once

// En-tete interne a src/app : il n'est pas installe et n'entre pas dans le SDK.
//
// Le seul objet partage entre les unites de traduction de MainWindow. Les
// icones d'action sont posees a la fois par les menus et par la table des
// outils ; les deux lecture doivent tomber d'accord, donc la fonction est ici
// en ligne plutot que dupliquee. La table kTools, elle, reste privee de son
// .cpp : exposee dans un en-tete, le `const` lui donnerait une copie par
// unite de traduction et la synchronisation menu/ruban serait perdue sans que
// rien ne casse a la compilation.
//
// Ordre de recherche : l'icone SVG embarquee (`:/icons/<nom>.svg`, livree avec
// l'executable, donc identique hors-ligne sur tout poste — ADR-016), puis le
// theme du systeme, puis l'icone standard de Qt.

#include <QAction>
#include <QFile>
#include <QIcon>
#include <QStyle>
#include <QWidget>

namespace bcad::app {

inline void setActionIcon(QWidget* widget, QAction* action, QStyle::StandardPixmap fallback,
                          const QString& name) {
    const QString embedded = QStringLiteral(":/icons/%1.svg").arg(name);
    QIcon icon = QFile::exists(embedded) ? QIcon(embedded) : QIcon::fromTheme(name);
    if (icon.isNull()) icon = widget->style()->standardIcon(fallback);
    action->setIcon(icon);
}

} // namespace bcad::app
