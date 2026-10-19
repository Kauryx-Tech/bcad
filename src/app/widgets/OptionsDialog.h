// Boite Options (S-15), comme OPTIONS d'AutoCAD. Ses onglets suivent l'ordre
// d'AutoCAD ; seuls ceux qui portent deja un reglage sont montres (Affichage,
// Selection aujourd'hui). Chaque onglet est une page : elle se construit a
// partir des reglages courants et sait les relire (OptionsPage). OK / Annuler
// / Appliquer ; effet immediat sur le canevas. Ajouter un reglage : voir
// detail/UserOptions.h.

#pragma once

#include "detail/UserOptions.h"

#include <QDialog>

#include <functional>
#include <vector>

class QTabWidget;

namespace bcad::app {

// Une page de la boite : son titre d'onglet, son contenu, et la relecture de
// ses champs dans les reglages.
struct OptionsPage {
    QString title;
    QWidget* widget = nullptr;
    std::function<void(detail::UserOptions&)> collect;
};

class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    OptionsDialog(const detail::UserOptions& current, QWidget* parent = nullptr);
    detail::UserOptions options() const;

signals:
    // OK ou Appliquer : la fenetre applique au canevas et enregistre.
    void applied(const detail::UserOptions& options);

private:
    detail::UserOptions current_;
    std::vector<OptionsPage> pages_;
};

// Reglages gardes par utilisateur (QSettings, groupe « options »).
detail::UserOptions loadUserOptions();
void saveUserOptions(const detail::UserOptions& options);

} // namespace bcad::app
