#include "widgets/PropertiesGeometrySection.h"

#include "editing/EditBatch.h"
#include "theme/Theme.h"

#include "bcad/core/Document.h"

#include <QComboBox>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <functional>
#include <memory>
#include <optional>

namespace bcad::app {

QString geometryFieldObjectName(detail::GeometryField field) {
    switch (field) {
        case detail::GeometryField::StartX: return QStringLiteral("geometryFieldStartX");
        case detail::GeometryField::StartY: return QStringLiteral("geometryFieldStartY");
        case detail::GeometryField::EndX: return QStringLiteral("geometryFieldEndX");
        case detail::GeometryField::EndY: return QStringLiteral("geometryFieldEndY");
        case detail::GeometryField::CenterX: return QStringLiteral("geometryFieldCenterX");
        case detail::GeometryField::CenterY: return QStringLiteral("geometryFieldCenterY");
        case detail::GeometryField::Radius: return QStringLiteral("geometryFieldRadius");
        case detail::GeometryField::StartAngle: return QStringLiteral("geometryFieldStartAngle");
        case detail::GeometryField::EndAngle: return QStringLiteral("geometryFieldEndAngle");
        case detail::GeometryField::PositionX: return QStringLiteral("geometryFieldPositionX");
        case detail::GeometryField::PositionY: return QStringLiteral("geometryFieldPositionY");
        case detail::GeometryField::VertexX: return QStringLiteral("geometryFieldVertexX");
        case detail::GeometryField::VertexY: return QStringLiteral("geometryFieldVertexY");
        case detail::GeometryField::Length: return QStringLiteral("geometryFieldLength");
        case detail::GeometryField::ArcLength: return QStringLiteral("geometryFieldArcLength");
        case detail::GeometryField::Diameter: return QStringLiteral("geometryFieldDiameter");
        case detail::GeometryField::Area: return QStringLiteral("geometryFieldArea");
    }
    return QString();
}

namespace {

// Champ de saisie qui sait renoncer : Échap remet la valeur du document et
// efface le refus, sans pousser quoi que ce soit dans l'historique ni perdre
// la sélection (docs/DESIGN_SYSTEM.md : « annuler la saisie »).
class GeometryFieldEdit : public QLineEdit {
public:
    explicit GeometryFieldEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}
    std::function<void()> onCancel;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            if (onCancel) onCancel();
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

} // namespace

PropertiesGeometrySection::PropertiesGeometrySection(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(theme::kSpaceS);
    layout_ = layout;

    title_ = new QLabel(tr("Géométrie"), this);
    title_->setObjectName("propertiesGeometryTitle");
    layout->addWidget(title_);

    // Lignes absentes tant qu'aucun objet n'est désigné : la section est
    // masquée par défaut, le titre avec.
    hide();
}

void PropertiesGeometrySection::setEntity(core::Document* doc, geom::Entity* entity, QUndoStack* stack) {
    doc_ = doc;
    stack_ = stack;
    const int id = entity ? entity->id() : -1;
    const std::vector<detail::GeometryField> fields =
        entity ? detail::geometryFields(*entity) : std::vector<detail::GeometryField>{};
    const std::size_t vertexCount = entity ? detail::geometryVertexCount(*entity) : 0;

    const bool objectChanged = id != entityId_;
    if (objectChanged) vertex_ = 0; // un autre objet reprend au premier sommet
    entityId_ = id;
    if (objectChanged || fields != fields_ || vertexCount != vertexCount_) rebuild();
    else updateValues();
}

geom::Entity* PropertiesGeometrySection::currentEntity() const {
    if (!doc_ || entityId_ < 0) return nullptr;
    return doc_->findEntity(entityId_);
}

void PropertiesGeometrySection::rebuild() {
    geom::Entity* entity = currentEntity();
    fields_ = entity ? detail::geometryFields(*entity) : std::vector<detail::GeometryField>{};

    // Le focus éventuel se pose encore sur un champ de l'ancien objet : on le
    // retrouve après reconstruction pour que la saisie en cours ne soit pas
    // perdue (changement d'objet, annulation pendant une édition).
    std::optional<detail::GeometryField> focused;
    for (const Row& row : rows_)
        if (row.edit && row.edit->hasFocus()) focused = row.field;

    if (rowsHost_) {
        layout_->removeWidget(rowsHost_);
        delete rowsHost_; // emporte libellés et champs, sans rien laisser orphelin
        rowsHost_ = nullptr;
        rowsForm_ = nullptr;
        vertexCombo_ = nullptr;
        rows_.clear();
    }

    if (!entity || fields_.empty()) {
        hide();
        return;
    }

    vertexCount_ = detail::geometryVertexCount(*entity);
    if (vertex_ >= static_cast<int>(vertexCount_)) vertex_ = vertexCount_ > 0 ? static_cast<int>(vertexCount_) - 1 : 0;

    rowsHost_ = new QWidget(this);
    rowsForm_ = new QFormLayout(rowsHost_);
    layout_->insertWidget(1, rowsHost_);

    for (const detail::GeometryField field : fields_) {
        if (field == detail::GeometryField::VertexX) {
            vertexCombo_ = new QComboBox(rowsHost_);
            vertexCombo_->setObjectName("geometryFieldVertexSelector");
            for (std::size_t i = 0; i < vertexCount_; ++i)
                vertexCombo_->addItem(tr("Sommet %1").arg(static_cast<int>(i) + 1), static_cast<int>(i));
            vertexCombo_->setCurrentIndex(vertex_);
            connect(vertexCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                if (index < 0) return;
                vertex_ = index;
                updateValues();
            });
            rowsForm_->addRow(tr("Sommet"), vertexCombo_);
        }

        const bool readOnly = detail::isReadOnlyGeometryField(field);
        auto* edit = new GeometryFieldEdit(rowsHost_);
        edit->setObjectName(geometryFieldObjectName(field));
        edit->setEnabled(!readOnly); // champ calculé : visible, désactivé
        if (!readOnly) {
            auto* fieldEdit = static_cast<GeometryFieldEdit*>(edit);
            fieldEdit->onCancel = [this, edit] {
                updateValues();
                clearError(edit);
            };
            connect(edit, &QLineEdit::editingFinished, this, [this, row = Row{field, edit}] { commit(row); });
        }
        rowsForm_->addRow(labelFor(field), edit);
        rows_.push_back(Row{field, edit});
    }

    updateValues();
    show();
    if (focused) {
        for (const Row& row : rows_) {
            if (row.field != *focused) continue;
            row.edit->setFocus();
            row.edit->end(false);
            break;
        }
    }
}

void PropertiesGeometrySection::updateValues() {
    geom::Entity* entity = currentEntity();
    if (!entity) return;
    for (const Row& row : rows_) {
        const std::optional<double> value = detail::geometryFieldValue(*entity, row.field, vertex_);
        if (!value) continue;
        const QString text = QString::fromStdString(detail::formatFieldValue(*value));
        if (row.edit->text() != text) row.edit->setText(text);
    }
}

void PropertiesGeometrySection::commit(Row row) {
    geom::Entity* entity = currentEntity();
    if (!entity) return;

    const std::optional<double> typed = detail::parseFieldValue(row.edit->text().toStdString());
    if (!typed) {
        showError(row.edit, tr("Valeur non numérique."));
        return;
    }
    const std::optional<double> current = detail::geometryFieldValue(*entity, row.field, vertex_);
    if (current && detail::sameFieldValue(*current, *typed)) {
        clearError(row.edit); // on retape ce qui est déjà affiché : rien à écrire
        return;
    }

    detail::GeometryEditReject reject = detail::GeometryEditReject::None;
    std::unique_ptr<geom::Entity> replacement =
        detail::withGeometryField(*entity, row.field, vertex_, *typed, &reject);
    if (!replacement) {
        showError(row.edit, rejectMessage(reject));
        return;
    }

    {
        // Une seule macro d'annulation : EditBatch la referme à sa destruction.
        EditBatch batch(doc_, stack_, tr("Modifier la géométrie"));
        batch.replace(entity, std::move(replacement));
    }
    clearError(row.edit);
    updateValues(); // sur place : le champ garderait sinon son ancien texte
    emit entityReplaced();
}

void PropertiesGeometrySection::showError(QLineEdit* edit, const QString& message) {
    clearError(edit);
    edit->setStyleSheet(
        QStringLiteral("QLineEdit { border: 1px solid %1; }")
            .arg(theme::color(theme::Token::SeverityError).name()));
    emit statusMessage(message);
    edit->setFocus();
    edit->selectAll();
}

void PropertiesGeometrySection::clearError(QLineEdit* edit) {
    if (!edit->styleSheet().isEmpty()) edit->setStyleSheet(QString());
}

QString PropertiesGeometrySection::labelFor(detail::GeometryField field) {
    switch (field) {
        case detail::GeometryField::StartX: return tr("Début X");
        case detail::GeometryField::StartY: return tr("Début Y");
        case detail::GeometryField::EndX: return tr("Fin X");
        case detail::GeometryField::EndY: return tr("Fin Y");
        case detail::GeometryField::CenterX: return tr("Centre X");
        case detail::GeometryField::CenterY: return tr("Centre Y");
        case detail::GeometryField::Radius: return tr("Rayon");
        case detail::GeometryField::StartAngle: return tr("Angle de départ");
        case detail::GeometryField::EndAngle: return tr("Angle de fin");
        case detail::GeometryField::PositionX:
        case detail::GeometryField::VertexX: return tr("X");
        case detail::GeometryField::PositionY:
        case detail::GeometryField::VertexY: return tr("Y");
        case detail::GeometryField::Length: return tr("Longueur");
        case detail::GeometryField::ArcLength: return tr("Longueur d'arc");
        case detail::GeometryField::Diameter: return tr("Diamètre");
        case detail::GeometryField::Area: return tr("Aire");
    }
    return QString();
}

QString PropertiesGeometrySection::rejectMessage(detail::GeometryEditReject reject) {
    switch (reject) {
        case detail::GeometryEditReject::None: return QString();
        case detail::GeometryEditReject::NotFinite: return tr("Valeur non numérique.");
        case detail::GeometryEditReject::RadiusNotPositive: return tr("Le rayon doit être supérieur à 0.");
        case detail::GeometryEditReject::Degenerate:
            return tr("Le début et la fin ne peuvent pas être confondus.");
        case detail::GeometryEditReject::InvalidVertex: return tr("Sommet hors de la polyligne.");
        case detail::GeometryEditReject::NotApplicable: return tr("Ce champ ne s'applique pas à cet objet.");
        case detail::GeometryEditReject::ReadOnly: return tr("Cette valeur est calculée.");
    }
    return QString();
}

} // namespace bcad::app
