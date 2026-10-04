#pragma once

// En-tete interne a src/app : il n'est pas installe et n'entre pas dans le SDK.
//
// Boites de message en TEXTE BRUT. QMessageBox interprete par defaut un texte
// qui ressemble a du HTML (Qt::AutoText) ; or ses messages portent des noms
// de fichier, des diagnostics de lecteur et des extraits de fichiers recus. Un
// fichier piege pourrait donc injecter mise en forme, liens ou images dans la
// boite. Toute boite de l'application passe par ici.

#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace bcad::app {

inline QMessageBox::StandardButton plainMessage(
    QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) {
    QMessageBox box(icon, title, text, buttons, parent);
    box.setTextFormat(Qt::PlainText);
    if (defaultButton != QMessageBox::NoButton) box.setDefaultButton(defaultButton);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

inline void warnPlain(QWidget* parent, const QString& title, const QString& text) {
    plainMessage(parent, QMessageBox::Warning, title, text);
}

inline void informPlain(QWidget* parent, const QString& title, const QString& text) {
    plainMessage(parent, QMessageBox::Information, title, text);
}

inline QMessageBox::StandardButton askPlain(
    QWidget* parent, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) {
    return plainMessage(parent, QMessageBox::Question, title, text, buttons, defaultButton);
}

} // namespace bcad::app
