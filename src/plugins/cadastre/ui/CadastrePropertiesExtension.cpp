#include <QWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include "bcad/geometry/Entity.h"
#include "bcad/properties/PropertyMap.h"

namespace bcad::cadastre {

class CadastrePropertiesExtension {
public:
    static QWidget* createEditorForParcel(QWidget* parent, geom::Entity* entity) {
        auto* widget = new QWidget(parent);
        auto* form = new QFormLayout(widget);
        
        auto* sectionEdit = new QLineEdit(widget);
        sectionEdit->setText(QString::fromStdString(entity->properties().getString("cadastre.section")));
        form->addRow("Section", sectionEdit);
        
        auto* numeroEdit = new QLineEdit(widget);
        numeroEdit->setText(QString::fromStdString(entity->properties().getString("cadastre.numero")));
        form->addRow("Numéro", numeroEdit);
        
        auto* contenanceEdit = new QLineEdit(widget);
        contenanceEdit->setText(QString::fromStdString(entity->properties().getString("cadastre.contenance")));
        form->addRow("Contenance", contenanceEdit);
        
        auto* communeEdit = new QLineEdit(widget);
        communeEdit->setText(QString::fromStdString(entity->properties().getString("cadastre.commune")));
        form->addRow("Commune", communeEdit);
        
        auto* proprietaireEdit = new QLineEdit(widget);
        proprietaireEdit->setText(QString::fromStdString(entity->properties().getString("cadastre.proprietaire")));
        form->addRow("Propriétaire", proprietaireEdit);
        
        auto* natureCombo = new QComboBox(widget);
        natureCombo->addItems({"Bâtie", "Non bâtie", "Agricole", "Domaine public"});
        natureCombo->setCurrentIndex(entity->properties().getEnum("cadastre.nature"));
        form->addRow("Nature", natureCombo);
        
        return widget;
    }
};

class ParcelValidationPanel : public QWidget {
public:
    explicit ParcelValidationPanel(QWidget* parent = nullptr) : QWidget(parent) {}
    void validateSelection(const std::vector<geom::Entity*>&) {}
};

class PlanGenerationDialog : public QWidget {
public:
    explicit PlanGenerationDialog(QWidget* parent = nullptr) : QWidget(parent) {}
    void generate(const std::vector<geom::Entity*>&) {}
};

} // namespace bcad::cadastre