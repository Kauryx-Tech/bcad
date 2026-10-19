#pragma once

#include <QComboBox>

namespace bcad::core { class Document; }

class QStandardItemModel;

namespace bcad::app {

// Liste des calques du ruban (L-02), comme celle du panneau Calques d'AutoCAD :
// chaque ligne montre une ampoule (allume / eteint), un cadenas (verrouille),
// la couleur et le nom. Un clic sur l'ampoule ou le cadenas bascule l'etat
// sans refermer la liste ; choisir un nom rend le calque courant. Les
// changements passent par LayerManager, qui previent le panneau Calques et
// le canevas.
class LayerCombo : public QComboBox {
    Q_OBJECT
public:
    explicit LayerCombo(QWidget* parent = nullptr);

    void setDocument(core::Document* doc);

    // Zones cliquables d'une ligne, en pixels depuis son bord gauche.
    static constexpr int kBulbX = 4, kLockX = 22, kColorX = 40, kTextX = 60, kZone = 16;

public slots:
    // Relit les calques : en place si la liste n'a pas change de noms (une
    // bascule depuis la liste ouverte ne la referme pas), sinon reconstruite.
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void onActivated(int row);

    core::Document* doc_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    bool updating_ = false;
};

} // namespace bcad::app
