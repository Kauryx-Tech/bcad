// Commande des poignees (M-02), comme « ** ETIRER ** » d'AutoCAD : Entree ou
// Espace fait defiler Etirer, Deplacer, Rotation, Echelle, Miroir ; options
// tapees [Point de base/Copier/annUler/Reference/Quitter], et sur une poignee
// de polyligne [Ajouter un sommet/Supprimer le sommet] ; le clic droit ouvre
// le menu des poignees (S-08d). Copier garde les originaux et repete jusqu'a
// Entree ; chaque pose est une annulation, annUler retire la derniere copie.

#include "viewport/Viewport.h"

#include "editing/EditBatch.h"

#include <QCursor>
#include <QMenu>
#include <cmath>
#include <numbers>

namespace bcad::app {

using geom::Point2;
using detail::GripMode;

namespace {

QString modeName(GripMode mode) {
    switch (mode) {
        case GripMode::Stretch: return Viewport::tr("ÉTIRER");
        case GripMode::Move: return Viewport::tr("DÉPLACER");
        case GripMode::Rotate: return Viewport::tr("ROTATION");
        case GripMode::Scale: return Viewport::tr("ÉCHELLE");
        case GripMode::Mirror: return Viewport::tr("MIROIR");
    }
    return {};
}

QString modeLabel(GripMode mode) {
    switch (mode) {
        case GripMode::Stretch: return Viewport::tr("Étirer");
        case GripMode::Move: return Viewport::tr("Déplacer");
        case GripMode::Rotate: return Viewport::tr("Rotation");
        case GripMode::Scale: return Viewport::tr("Échelle");
        case GripMode::Mirror: return Viewport::tr("Miroir");
    }
    return {};
}

bool usesReference(GripMode mode) { return mode == GripMode::Rotate || mode == GripMode::Scale; }

} // namespace

QString Viewport::gripPrompt() const {
    const QString head = gripCopy_ ? tr("** %1 (plusieurs) **").arg(modeName(gripMode_))
                                   : tr("** %1 **").arg(modeName(gripMode_));
    if (gripAwaitBase_) return tr("%1 Spécifiez le point de base").arg(head);
    if (gripAwaitRef_) {
        return gripMode_ == GripMode::Rotate
                   ? tr("%1 Spécifiez l'angle de référence (degrés ou point)").arg(head)
                   : tr("%1 Spécifiez la longueur de référence (nombre ou point)").arg(head);
    }
    if (gripAddVertex_) return tr("%1 Spécifiez le nouveau sommet").arg(head);
    QString ask;
    switch (gripMode_) {
        case GripMode::Stretch: ask = tr("Spécifiez le point d'étirement"); break;
        case GripMode::Move: ask = tr("Spécifiez le point de déplacement"); break;
        case GripMode::Rotate: ask = tr("Spécifiez l'angle de rotation"); break;
        case GripMode::Scale: ask = tr("Spécifiez le facteur d'échelle"); break;
        case GripMode::Mirror: ask = tr("Spécifiez le deuxième point"); break;
    }
    QStringList options{tr("Point de base"), tr("Copier"), tr("annUler")};
    if (usesReference(gripMode_)) options << tr("Référence");
    if (gripMode_ == GripMode::Stretch && singlePolylineGrip()) {
        options << tr("Ajouter un sommet");
        if (singlePolylineGrip()->second.role == detail::GripRole::Vertex) options << tr("Supprimer le sommet");
    }
    options << tr("Quitter");
    return tr("%1 %2 ou [%3]").arg(head, ask, options.join(QLatin1Char('/')));
}

void Viewport::gripEnter(bool fromRightClick) {
    if (fromRightClick) {
        showGripMenu();
        return;
    }
    // Comme AutoCAD : en mode plusieurs (Copier), Entree termine ; une option
    // en attente est abandonnee ; sinon le mode suivant.
    if (gripCopy_) {
        endCommand();
        return;
    }
    if (gripAwaitBase_ || gripAwaitRef_ || gripAddVertex_) {
        gripAwaitBase_ = gripAwaitRef_ = gripAddVertex_ = false;
    } else {
        gripMode_ = detail::nextGripMode(gripMode_);
        gripRef_.reset();
    }
    notifyPrompt();
    update();
}

bool Viewport::submitGripOption(const QString& text) {
    if (tool_ != ToolMode::GripStretch) return false;
    const QString option = text.trimmed().toUpper();
    if (option.isEmpty()) return false;   // Entree : gripEnter
    auto is = [&](const char* letter) { return option == QLatin1String(letter); };
    if (is("P")) {
        gripAwaitBase_ = true;
    } else if (is("C")) {
        gripCopy_ = !gripCopy_;
    } else if (is("U")) {
        undoGripCopy();
    } else if (is("Q")) {
        endCommand();
        return true;
    } else if (is("R") && usesReference(gripMode_)) {
        gripAwaitRef_ = true;
    } else if (is("A") && gripMode_ == GripMode::Stretch && singlePolylineGrip()) {
        gripAddVertex_ = true;
    } else if (is("S") && gripMode_ == GripMode::Stretch && singlePolylineGrip()) {
        const auto [entity, grip] = *singlePolylineGrip();
        GripChanges changes;
        if (auto out = detail::removeVertex(*entity, grip)) changes.emplace_back(entity, std::move(out));
        gripCopy_ = false;
        commitGrip(std::move(changes), tr("Supprimer le sommet"));
        return true;
    } else {
        // Un nombre : angle (degres) de Rotation, facteur d'Echelle, ou la
        // reference attendue. Le reste (@dx,dy, X,Y) est un point.
        bool ok = false;
        const double value = option.toDouble(&ok);
        if (!ok || !(gripAwaitRef_ || usesReference(gripMode_))) return false;
        const Point2 base = toolPoints_.front();
        const bool rotate = gripMode_ == GripMode::Rotate;
        if (gripAwaitRef_) {
            if (!rotate && !(value > 0.0)) {
                emit statusMessage(tr("Échelle : la longueur de référence doit être positive."));
                return true;
            }
            gripRef_ = rotate ? value * std::numbers::pi / 180.0 : value;
            gripAwaitRef_ = false;
        } else if (rotate) {
            commitGrip(transformChanges(geom::Transform2D::rotation(
                           value * std::numbers::pi / 180.0 - gripRef_.value_or(0.0), base)),
                       modeLabel(gripMode_));
            return true;
        } else {
            const double factor = gripRef_ ? value / *gripRef_ : value;
            if (!(factor > 0.0)) {
                emit statusMessage(tr("Échelle : le facteur doit être positif."));
                return true;
            }
            commitGrip(transformChanges(geom::Transform2D::scaling(factor, base)), modeLabel(gripMode_));
            return true;
        }
    }
    notifyPrompt();
    update();
    return true;
}

void Viewport::applyGripPoint(const Point2& world) {
    gripDragArmed_ = false;
    if (toolPoints_.empty()) return;
    const Point2 base = toolPoints_.front();
    if (gripAwaitBase_) {
        toolPoints_ = {world};
        gripAwaitBase_ = false;
        notifyPrompt();
        return;
    }
    if (gripAwaitRef_) {
        const double d = geom::distance(base, world);
        if (d < geom::Tolerance::kDegenerateLength) return;
        gripRef_ = gripMode_ == GripMode::Rotate ? geom::angleOf(base, world) : d;
        gripAwaitRef_ = false;
        notifyPrompt();
        return;
    }
    if (gripAddVertex_) {
        GripChanges changes;
        if (const auto target = singlePolylineGrip())
            if (auto out = detail::insertVertex(*target->first, target->second, world))
                changes.emplace_back(target->first, std::move(out));
        gripCopy_ = false;
        commitGrip(std::move(changes), tr("Ajouter un sommet"));
        return;
    }
    if (geom::distance(base, world) < geom::Tolerance::kDegenerateLength) {
        if (!gripCopy_) endCommand();   // clic sur la poignee elle-meme : rien
        return;
    }
    if (gripMode_ == GripMode::Stretch) {
        commitGrip(stretchChanges(world), modeLabel(gripMode_));
    } else if (const auto t = gripPointTransform(world)) {
        commitGrip(transformChanges(*t), modeLabel(gripMode_));
    }
}

void Viewport::commitGrip(GripChanges changes, const QString& label) {
    if (changes.empty()) {
        emit statusMessage(tr("%1 : forme impossible (rayon nul, points alignés ou trop peu de sommets).").arg(label));
        if (!gripCopy_) endCommand();
        return;
    }
    {
        EditBatch batch(doc_, undoStack_, label);
        for (auto& [original, result] : changes) {
            if (gripCopy_) {
                result->selected = false;   // la copie s'ajoute, l'original reste choisi
                batch.add(std::move(result));
            } else {
                batch.replace(original, std::move(result));
            }
        }
    }
    if (gripCopy_) {
        ++gripCopies_;
        notifyPrompt();
        update();
        return;
    }
    endCommand();
    emit selectionChanged();
}

void Viewport::undoGripCopy() {
    if (gripCopies_ == 0 || !undoStack_) {
        emit statusMessage(tr("annUler : aucune copie à retirer."));
        return;
    }
    undoStack_->undo();
    --gripCopies_;
}

void Viewport::showGripMenu() {
    // Menu des poignees d'AutoCAD : chaque entree fait ce que fait l'option
    // tapee correspondante.
    QMenu menu(this);
    menu.addAction(tr("Entrée"), this, [this] { gripEnter(false); });
    menu.addSeparator();
    for (GripMode mode : {GripMode::Stretch, GripMode::Move, GripMode::Rotate, GripMode::Scale, GripMode::Mirror}) {
        QAction* action = menu.addAction(modeLabel(mode), this, [this, mode] {
            gripMode_ = mode;
            gripRef_.reset();
            gripAwaitRef_ = gripAddVertex_ = false;
            notifyPrompt();
        });
        action->setCheckable(true);
        action->setChecked(mode == gripMode_);
        if (mode == GripMode::Stretch && singlePolylineGrip()) {
            menu.addAction(tr("Ajouter un sommet"), this, [this] { submitGripOption(QStringLiteral("A")); });
            if (singlePolylineGrip()->second.role == detail::GripRole::Vertex)
                menu.addAction(tr("Supprimer le sommet"), this, [this] { submitGripOption(QStringLiteral("S")); });
        }
    }
    menu.addSeparator();
    menu.addAction(tr("Point de base"), this, [this] { submitGripOption(QStringLiteral("P")); });
    QAction* copy = menu.addAction(tr("Copier"), this, [this] { submitGripOption(QStringLiteral("C")); });
    copy->setCheckable(true);
    copy->setChecked(gripCopy_);
    if (usesReference(gripMode_))
        menu.addAction(tr("Référence"), this, [this] { submitGripOption(QStringLiteral("R")); });
    if (gripCopies_ > 0) menu.addAction(tr("Annuler"), this, [this] { undoGripCopy(); });
    menu.addSeparator();
    menu.addAction(tr("Propriétés..."), this, [this] { emit propertiesRequested(); });
    menu.addSeparator();
    menu.addAction(tr("Quitter"), this, [this] { endCommand(); });
    menu.exec(QCursor::pos());
    update();
}

} // namespace bcad::app
