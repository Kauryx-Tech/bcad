// Les dessins ouverts, un par onglet, comme les onglets de fichiers d'AutoCAD.
//
// Chaque onglet porte une DocumentSession : document, historique, chemin, etat
// modifie, cadrage. Activer un onglet rebranche la vue, les panneaux et les
// actions Annuler / Retablir sur ce dessin, et sur lui seul. L'ordre des
// onglets et celui des sessions restent le meme : glisser un onglet deplace sa
// session.

#include "MainWindow.h"

#include "LayerPanel.h"
#include "PropertiesPanel.h"
#include "bcad/io/Database.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>

namespace bcad::app {

// Les historiques des dessins sont relies a la fenetre (etat modifie, libelle
// d'onglet). Les membres de la fenetre — donc les dessins et leurs historiques
// — sont detruits AVANT que QObject ne coupe ces connexions : un historique
// emet encore indexChanged en se vidant, et la fenetre parcourait alors la
// liste des dessins en cours de destruction (memoire liberee, plantage
// aleatoire a la fermeture). Les connexions sont donc coupees d'abord.
MainWindow::~MainWindow() {
    for (int i = 0; i < sessions_.count(); ++i)
        QObject::disconnect(sessions_.at(i).undoStack.get(), nullptr, this, nullptr);
}

QWidget* MainWindow::buildDocumentTabs() {
    auto* strip = new QWidget(this);
    strip->setObjectName("documentTabsStrip");
    auto* layout = new QHBoxLayout(strip);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(2);

    documentTabs_ = new QTabBar(strip);
    documentTabs_->setObjectName("documentTabs");
    documentTabs_->setDocumentMode(true);
    documentTabs_->setTabsClosable(true);
    documentTabs_->setMovable(true);
    documentTabs_->setExpanding(false);
    documentTabs_->setElideMode(Qt::ElideMiddle);
    documentTabs_->setUsesScrollButtons(true);
    documentTabs_->setFocusPolicy(Qt::NoFocus);

    auto* newTab = new QToolButton(strip);
    newTab->setObjectName("documentNewTab");
    newTab->setText(QStringLiteral("+"));
    newTab->setToolTip(tr("Nouveau dessin (Ctrl+N)"));
    newTab->setAutoRaise(true);
    connect(newTab, &QToolButton::clicked, this, &MainWindow::onNew);

    layout->addWidget(documentTabs_);
    layout->addWidget(newTab);
    layout->addStretch(1);

    connect(documentTabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index != activeSession_) activateSession(index);
    });
    connect(documentTabs_, &QTabBar::tabCloseRequested, this,
            [this](int index) { closeSession(index); });
    connect(documentTabs_, &QTabBar::tabMoved, this, [this](int from, int to) {
        sessions_.move(from, to);
        if (activeSession_ == from) activeSession_ = to;
        else if (from < activeSession_ && to >= activeSession_) --activeSession_;
        else if (from > activeSession_ && to <= activeSession_) ++activeSession_;
    });
    return strip;
}

int MainWindow::addSessionTab(int sessionIndex) {
    DocumentSession& added = sessions_.at(sessionIndex);
    added.undoStack->setUndoLimit(100);
    undoGroup_.addStack(added.undoStack.get());
    // Toute commande poussee dans l'historique de ce dessin le marque modifie,
    // qu'il soit l'onglet actif ou non. Mis a jour sur indexChanged plutot que
    // par isClean() : un retour a l'etat initial par Annuler laisse donc la
    // marque, imprecision assumee pour un garde-fou contre la perte de travail.
    DocumentSession* raw = &added;
    connect(added.undoStack.get(), &QUndoStack::indexChanged, this, [this, raw] {
        raw->dirty = true;
        updateTabLabel(sessions_.indexOf(raw));
        updateWindowTitle();
    });
    const QSignalBlocker quiet(documentTabs_);
    documentTabs_->insertTab(sessionIndex, added.tabLabel());
    updateTabLabel(sessionIndex);
    return sessionIndex;
}

int MainWindow::newUntitledSession() {
    const int index = sessions_.addUntitled(tr("Dessin"));
    applyStyleProvidersToDocument(*sessions_.at(index).document);
    return addSessionTab(index);
}

void MainWindow::activateSession(int index) {
    if (index < 0 || index >= sessions_.count()) return;
    // Le cadrage du dessin quitte est garde : y revenir le retrouve tel quel.
    if (activeSession_ >= 0 && activeSession_ < sessions_.count() && activeSession_ != index)
        session().camera = viewport_->camera();

    activeSession_ = index;
    DocumentSession& active = session();
    document_ = active.document.get();
    undoStack_ = active.undoStack.get();
    undoGroup_.setActiveStack(undoStack_);

    // Changer de dessin termine la commande en cours, saisie de contour
    // comprise : ses points appartenaient a l'autre dessin.
    viewport_->setTool(ToolMode::Select);
    viewport_->setDocument(document_);
    viewport_->setUndoStack(undoStack_);
    if (active.camera) viewport_->setCamera(*active.camera);
    else viewport_->zoomToFit();
    layerPanel_->setDocument(document_);
    propertiesPanel_->setDocument(document_);
    propertiesPanel_->setUndoStack(undoStack_);

    if (documentTabs_->currentIndex() != index) {
        const QSignalBlocker quiet(documentTabs_);
        documentTabs_->setCurrentIndex(index);
    }
    refreshDocumentViews();
}

bool MainWindow::closeSession(int index) {
    if (index < 0 || index >= sessions_.count()) return false;
    // La confirmation porte sur le dessin a fermer : il devient l'onglet actif
    // pour que l'utilisateur voie ce qu'il s'apprete a perdre.
    if (sessions_.at(index).dirty) {
        activateSession(index);
        if (!confirmDiscard()) return false;
    }
    if (sessions_.count() == 1) {
        // Jamais de fenetre sans dessin : le dernier onglet laisse place a un
        // dessin vierge.
        newUntitledSession();
    }
    const bool wasActive = index == activeSession_;
    {
        const QSignalBlocker quiet(documentTabs_);
        documentTabs_->removeTab(index);
    }
    // Couper les connexions de l'historique avant de detruire le dessin : en se
    // vidant, il emet encore indexChanged (voir ~MainWindow).
    QObject::disconnect(sessions_.at(index).undoStack.get(), nullptr, this, nullptr);
    undoGroup_.removeStack(sessions_.at(index).undoStack.get());
    sessions_.remove(index);
    if (activeSession_ > index) --activeSession_;
    if (wasActive) {
        activeSession_ = -1;
        activateSession(std::min(index, sessions_.count() - 1));
    }
    return true;
}

void MainWindow::updateTabLabel(int index) {
    if (index < 0 || index >= documentTabs_->count()) return;
    const DocumentSession& tab = sessions_.at(index);
    documentTabs_->setTabText(index, tab.tabLabel());
    documentTabs_->setTabToolTip(index, tab.filePath.isEmpty()
                                            ? tr("%1 — jamais enregistré").arg(tab.displayName())
                                            : tab.filePath);
}

bool MainWindow::openFile(const QString& path) {
    const int already = sessions_.indexOfPath(path);
    if (already >= 0) {
        activateSession(already);
        statusBar()->showMessage(tr("« %1 » est déjà ouvert").arg(QFileInfo(path).fileName()), 4000);
        return true;
    }

    const QString loadPath = resolveRecoveryPath(path);
    auto loaded = std::make_unique<DocumentSession>();
    std::vector<std::string> diag;
    if (!io::Database::load(loadPath.toStdString(), *loaded->document, &diag)) {
        QMessageBox::warning(this, tr("Ouverture impossible"),
                             tr("Impossible d'ouvrir « %1 ».").arg(loadPath));
        return false;
    }
    applyStyleProvidersToDocument(*loaded->document);
    // Le chemin reste le vrai fichier meme si la sauvegarde automatique a ete
    // chargee, pour que Ctrl+S ecrive dessus ; le dessin est alors marque
    // modifie pour que le contenu recupere ne soit pas perdu en silence.
    loaded->filePath = path;
    loaded->dirty = (loadPath != path);

    // Un dessin vierge et intact (le « Dessin1 » du demarrage) cede sa place
    // au fichier ouvert plutot que de laisser un onglet vide derriere lui.
    const int replaced = session().isPristine() ? activeSession_ : -1;
    const int index = addSessionTab(sessions_.add(std::move(loaded)));
    activateSession(index);
    if (replaced >= 0) closeSession(replaced);
    viewport_->zoomToFit();
    if (!diag.empty()) {
        statusBar()->showMessage(
            tr("%1 avertissement(s) au chargement de « %2 »")
                .arg(diag.size()).arg(QFileInfo(path).fileName()), 8000);
    }
    return true;
}

} // namespace bcad::app
