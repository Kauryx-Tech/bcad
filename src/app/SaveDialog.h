#pragma once

// En-tete interne a src/app : il n'est pas installe et n'entre pas dans le SDK.
//
// Boite « Enregistrer sous » commune au projet .bcad, a l'export DXF et aux
// exports des modules. Un nom tape sans extension (« plan ») produisait un
// fichier « plan » que la boite d'ouverture, filtree sur *.bcad, ne montrait
// plus. L'extension est donc ajoutee ; et comme la confirmation d'ecrasement
// de la boite ne portait que sur le nom tape, un fichier existant sous le nom
// complete est confirme ici.

#include "PlainMessage.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace bcad::app {

// `path` termine par `.extension` (sans tenir compte de la casse), en
// l'ajoutant s'il manque. `extension` s'ecrit sans le point.
inline QString withExtension(const QString& path, const QString& extension) {
    if (path.isEmpty() || extension.isEmpty()) return path;
    if (QFileInfo(path).suffix().compare(extension, Qt::CaseInsensitive) == 0) return path;
    return path + QLatin1Char('.') + extension;
}

// Chemin choisi, extension garantie ; vide si l'utilisateur renonce.
inline QString askSavePath(QWidget* parent, const QString& title, const QString& filter,
                           const QString& extension) {
    const QString typed = QFileDialog::getSaveFileName(parent, title, {}, filter);
    if (typed.isEmpty()) return {};
    const QString path = withExtension(typed, extension);
    if (path != typed && QFileInfo::exists(path)) {
        const auto reply = askPlain(
            parent, title,
            QObject::tr("« %1 » existe déjà. Le remplacer ?").arg(QFileInfo(path).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return {};
    }
    return path;
}

} // namespace bcad::app
