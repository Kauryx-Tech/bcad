#include "LayerPanel.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <sstream>

namespace bcad::app {

namespace {
constexpr int kColCurrent = 0;
constexpr int kColVisible = 1;
constexpr int kColLocked = 2;
constexpr int kColColor = 3;
constexpr int kColLineWeight = 4;
constexpr int kColLineType = 5;
constexpr int kColName = 6;

QColor toQColor(const geom::Color& c) {
    return QColor::fromRgbF(c.r, c.g, c.b, c.a);
}
geom::Color fromQColor(const QColor& c) {
    return geom::Color{ static_cast<float>(c.redF()), static_cast<float>(c.greenF()),
                         static_cast<float>(c.blueF()), static_cast<float>(c.alphaF()) };
}

const char* lineTypeName(layers::Layer::LineType t) {
    switch (t) {
        case layers::Layer::LineType::Continuous: return "Continuous";
        case layers::Layer::LineType::Dashed: return "Dashed";
        case layers::Layer::LineType::Dotted: return "Dotted";
        case layers::Layer::LineType::DashDot: return "DashDot";
    }
    return "Continuous";
}

std::string layerItemName(QTreeWidgetItem* item) {
    return item->data(kColName, Qt::UserRole).toString().toStdString();
}
} // namespace

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(tr("Rechercher un calque..."));
    layout->addWidget(searchEdit_);

    filterCombo_ = new QComboBox(this);
    filterCombo_->addItem(tr("Tous les calques"));
    filterCombo_->addItem(tr("Visibles uniquement"));
    filterCombo_->addItem(tr("Masqués uniquement"));
    filterCombo_->addItem(tr("Verrouillés uniquement"));
    filterCombo_->addItem(tr("Déverrouillés uniquement"));
    layout->addWidget(filterCombo_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(7);
    tree_->setHeaderLabels({ QString(), tr("Vis."), tr("Verrou"), tr("Couleur"), tr("Ép."), tr("Type"), tr("Calque") });
    tree_->header()->setSectionResizeMode(kColName, QHeaderView::Stretch);
    tree_->setColumnWidth(kColCurrent, 20);
    tree_->setColumnWidth(kColVisible, 32);
    tree_->setColumnWidth(kColLocked, 36);
    tree_->setColumnWidth(kColColor, 40);
    tree_->setColumnWidth(kColLineWeight, 40);
    tree_->setColumnWidth(kColLineType, 70);
    tree_->setRootIsDecorated(false);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tree_, 1);

    auto* buttons = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("+ Calque"), this);
    auto* removeBtn = new QPushButton(tr("- Calque"), this);
    saveStateBtn_ = new QPushButton(tr("Enregistrer l'état"), this);
    restoreStateBtn_ = new QPushButton(tr("Restaurer l'état"), this);
    buttons->addWidget(addBtn);
    buttons->addWidget(removeBtn);
    buttons->addStretch(1);
    buttons->addWidget(saveStateBtn_);
    buttons->addWidget(restoreStateBtn_);
    layout->addLayout(buttons);

    connect(addBtn, &QPushButton::clicked, this, &LayerPanel::onAddLayerClicked);
    connect(removeBtn, &QPushButton::clicked, this, &LayerPanel::onRemoveLayerClicked);
    connect(saveStateBtn_, &QPushButton::clicked, this, &LayerPanel::onSaveStateClicked);
    connect(restoreStateBtn_, &QPushButton::clicked, this, &LayerPanel::onRestoreStateClicked);
    connect(tree_, &QTreeWidget::itemChanged, this, &LayerPanel::onItemChanged);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &LayerPanel::onItemDoubleClicked);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &LayerPanel::onContextMenuRequested);
    connect(searchEdit_, &QLineEdit::textChanged, this, &LayerPanel::onSearchTextChanged);
    connect(filterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LayerPanel::onFilterChanged);
}

void LayerPanel::setDocument(core::Document* doc) {
    doc_ = doc;
    if (doc_) {
        doc_->layerManager().onChanged = [this] { refresh(); };
    }
    refresh();
}

bool LayerPanel::passesFilter(const layers::Layer& layer) const {
    const QString needle = searchEdit_->text().trimmed();
    if (!needle.isEmpty() && !QString::fromStdString(layer.name).contains(needle, Qt::CaseInsensitive)) {
        return false;
    }
    switch (filterCombo_->currentIndex()) {
        case FilterVisible: return layer.visible;
        case FilterHidden: return !layer.visible;
        case FilterLocked: return layer.locked;
        case FilterUnlocked: return !layer.locked;
        default: return true;
    }
}

void LayerPanel::applyFilters() {
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        const layers::Layer* layer = doc_ ? doc_->layerManager().find(layerItemName(item)) : nullptr;
        item->setHidden(!layer || !passesFilter(*layer));
    }
}

void LayerPanel::refresh() {
    if (!doc_) return;
    updating_ = true;
    tree_->clear();

    const std::string& currentName = doc_->layerManager().currentLayerName();
    for (const auto& layer : doc_->layerManager().layers()) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(kColVisible, layer.visible ? Qt::Checked : Qt::Unchecked);
        item->setCheckState(kColLocked, layer.locked ? Qt::Checked : Qt::Unchecked);

        QPixmap swatch(16, 16);
        swatch.fill(toQColor(layer.color));
        item->setIcon(kColColor, QIcon(swatch));

        item->setText(kColLineWeight, QString::number(layer.lineWeight, 'f', 2));
        item->setText(kColLineType, QString::fromLatin1(lineTypeName(layer.lineType)));
        item->setText(kColName, QString::fromStdString(layer.name));
        if (layer.name == "0") item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(kColName, Qt::UserRole, QString::fromStdString(layer.name));

        if (layer.name == currentName) {
            item->setText(kColCurrent, QString::fromUtf8("\xe2\x97\x8f")); // pastille
            QFont bold = item->font(kColName);
            bold.setBold(true);
            item->setFont(kColName, bold);
        }
    }
    updating_ = false;
    applyFilters();
}

void LayerPanel::onItemChanged(QTreeWidgetItem* item, int column) {
    if (updating_ || !doc_ || !item) return;
    std::string name = layerItemName(item);

    if (column == kColVisible) {
        doc_->layerManager().setVisible(name, item->checkState(kColVisible) == Qt::Checked);
    } else if (column == kColLocked) {
        doc_->layerManager().setLocked(name, item->checkState(kColLocked) == Qt::Checked);
    } else if (column == kColName) {
        std::string newName = item->text(kColName).toStdString();
        if (newName != name && !newName.empty()) {
            doc_->layerManager().renameLayer(name, newName);
        }
    }
}

void LayerPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (!doc_ || !item) return;
    std::string name = layerItemName(item);
    layers::Layer* layer = doc_->layerManager().find(name);
    if (!layer) return;

    if (column == kColCurrent) {
        setCurrentLayer(name);
        emit layerActivated(QString::fromStdString(name));
    } else if (column == kColColor) {
        QColor chosen = QColorDialog::getColor(toQColor(layer->color), this, tr("Layer color"));
        if (chosen.isValid()) {
            layer->color = fromQColor(chosen);
            refresh();
        }
    } else if (column == kColLineWeight) {
        bool ok = false;
        double lw = QInputDialog::getDouble(this, tr("Line weight"), tr("Width (mm):"), layer->lineWeight, 0.0,
                                            2.11, 2, &ok);
        if (ok) {
            layer->lineWeight = lw;
            refresh();
        }
    } else if (column == kColLineType) {
        // Cycle des types de trait au double-clic, comme un sélecteur rapide.
        auto lt = layer->lineType;
        lt = (lt == layers::Layer::LineType::Continuous) ? layers::Layer::LineType::Dashed
           : (lt == layers::Layer::LineType::Dashed)    ? layers::Layer::LineType::Dotted
           : (lt == layers::Layer::LineType::Dotted)    ? layers::Layer::LineType::DashDot
                                                        : layers::Layer::LineType::Continuous;
        layer->lineType = lt;
        refresh();
    } else if (column == kColName || column == kColCurrent) {
        showLayerProperties(name);
    }
}

void LayerPanel::onSearchTextChanged(const QString&) {
    applyFilters();
}

void LayerPanel::onFilterChanged(int) {
    applyFilters();
}

void LayerPanel::onAddLayerClicked() {
    if (!doc_) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New layer"), tr("Name:"), QLineEdit::Normal,
                                          tr("Layer"), &ok);
    if (ok && !name.isEmpty()) {
        doc_->layerManager().createLayer(name.toStdString());
    }
}

void LayerPanel::onRemoveLayerClicked() {
    if (!doc_) return;
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item) return;
    doc_->layerManager().removeLayer(layerItemName(item));
}

void LayerPanel::setCurrentLayer(const std::string& name) {
    if (doc_) doc_->layerManager().setCurrentLayer(name);
}

void LayerPanel::isolateLayer(const std::string& name) {
    if (!doc_) return;
    for (const auto& layer : doc_->layerManager().layers()) {
        doc_->layerManager().setVisible(layer.name, layer.name == name);
    }
}

void LayerPanel::showAllLayers() {
    if (!doc_) return;
    for (const auto& layer : doc_->layerManager().layers()) {
        doc_->layerManager().setVisible(layer.name, true);
    }
}

void LayerPanel::showLayerProperties(const std::string& name) {
    if (!doc_) return;
    layers::Layer* layer = doc_->layerManager().find(name);
    if (!layer) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Layer Properties — %1").arg(QString::fromStdString(name)));
    auto* form = new QFormLayout(&dialog);

    auto* nameLabel = new QLabel(QString::fromStdString(layer->name), &dialog);
    form->addRow(tr("Name"), nameLabel);

    auto* lwSpin = new QDoubleSpinBox(&dialog);
    lwSpin->setRange(0.0, 2.11);
    lwSpin->setSingleStep(0.05);
    lwSpin->setDecimals(2);
    lwSpin->setValue(layer->lineWeight);
    form->addRow(tr("Line weight (mm)"), lwSpin);

    auto* ltCombo = new QComboBox(&dialog);
    ltCombo->addItems({ "Continuous", "Dashed", "Dotted", "DashDot" });
    ltCombo->setCurrentText(QString::fromLatin1(lineTypeName(layer->lineType)));
    form->addRow(tr("Line type"), ltCombo);

    auto* colorBtn = new QPushButton(&dialog);
    QColor initial = toQColor(layer->color);
    colorBtn->setText(initial.name());
    colorBtn->setStyleSheet(QString("background-color: %1;").arg(initial.name()));
    auto* chosen = new QColor(initial);
    QObject::connect(colorBtn, &QPushButton::clicked, &dialog, [colorBtn, chosen] {
        QColor c = QColorDialog::getColor(*chosen, colorBtn, tr("Layer color"));
        if (c.isValid()) {
            *chosen = c;
            colorBtn->setText(c.name());
            colorBtn->setStyleSheet(QString("background-color: %1;").arg(c.name()));
        }
    });
    form->addRow(tr("Color"), colorBtn);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        layer->lineWeight = lwSpin->value();
        layer->lineType = ltCombo->currentIndex() == 1 ? layers::Layer::LineType::Dashed
                        : ltCombo->currentIndex() == 2 ? layers::Layer::LineType::Dotted
                        : ltCombo->currentIndex() == 3 ? layers::Layer::LineType::DashDot
                                                       : layers::Layer::LineType::Continuous;
        layer->color = fromQColor(*chosen);
        delete chosen;
        refresh();
    } else {
        delete chosen;
    }
}

void LayerPanel::onContextMenuRequested(const QPoint& pos) {
    QTreeWidgetItem* item = tree_->itemAt(pos);
    if (!item || !doc_) return;
    std::string name = layerItemName(item);

    QMenu menu(this);
    QAction* setCurrentAct = menu.addAction(tr("Set as current layer"));
    QAction* isolateAct = menu.addAction(tr("Isolate layer"));
    QAction* showAllAct = menu.addAction(tr("Show all layers"));
    menu.addSeparator();
    QAction* propertiesAct = menu.addAction(tr("Properties..."));

    QAction* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
    if (chosen == setCurrentAct) {
        setCurrentLayer(name);
    } else if (chosen == isolateAct) {
        isolateLayer(name);
    } else if (chosen == showAllAct) {
        showAllLayers();
    } else if (chosen == propertiesAct) {
        showLayerProperties(name);
    }
}

std::string LayerPanel::serializeStates(const std::vector<layers::Layer>& layers) {
    std::ostringstream ss;
    for (const auto& l : layers) {
        ss << l.name << ';' << l.visible << ';' << l.locked << ';'
           << l.color.r << ',' << l.color.g << ',' << l.color.b << ',' << l.color.a << ';'
           << l.lineWeight << ';' << static_cast<int>(l.lineType) << '\n';
    }
    return ss.str();
}

void LayerPanel::applyState(const std::string& data) {
    if (!doc_) return;
    std::istringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string name, colorPart, lwPart, ltPart;
        int visible = 1, locked = 0, lt = 0;
        double lw = 0.25;
        float r = 1, g = 1, b = 1, a = 1;
        if (!std::getline(ls, name, ';')) continue;
        ls >> visible; ls.ignore(1);
        ls >> locked; ls.ignore(1);
        std::getline(ls, colorPart, ';');
        ls >> lw; ls.ignore(1);
        ls >> lt;
        std::sscanf(colorPart.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a);
        layers::Layer* layer = doc_->layerManager().find(name);
        if (!layer) continue;
        layer->visible = visible != 0;
        layer->locked = locked != 0;
        layer->color = geom::Color{ r, g, b, a };
        layer->lineWeight = lw;
        layer->lineType = static_cast<layers::Layer::LineType>(lt);
    }
    refresh();
}

void LayerPanel::onSaveStateClicked() {
    if (!doc_) return;
    QSettings settings;
    settings.beginGroup("layerStates");
    QStringList existing = settings.childKeys();

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save layer state"), tr("State name:"), QLineEdit::Normal,
                                          tr("State"), &ok);
    if (!ok || name.isEmpty()) return;
    if (existing.contains(name)) {
        auto reply = QMessageBox::question(this, tr("Overwrite"), tr("State '%1' exists. Overwrite?").arg(name));
        if (reply != QMessageBox::Yes) return;
    }
    settings.setValue(name, QString::fromStdString(serializeStates(doc_->layerManager().layers())));
}

void LayerPanel::onRestoreStateClicked() {
    if (!doc_) return;
    QSettings settings;
    settings.beginGroup("layerStates");
    QStringList existing = settings.childKeys();
    if (existing.isEmpty()) {
        QMessageBox::information(this, tr("Layer states"), tr("No saved layer states."));
        return;
    }
    bool ok = false;
    QString name = QInputDialog::getItem(this, tr("Restore layer state"), tr("State:"), existing, 0, false, &ok);
    if (!ok || name.isEmpty()) return;
    applyState(settings.value(name).toString().toStdString());
}

} // namespace bcad::app
