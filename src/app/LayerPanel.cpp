#include "bcad/app/LayerPanel.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace bcad::app {

namespace {
constexpr int kColVisible = 0;
constexpr int kColLocked = 1;
constexpr int kColColor = 2;
constexpr int kColName = 3;

QColor toQColor(const geom::Color& c) {
    return QColor::fromRgbF(c.r, c.g, c.b, c.a);
}
geom::Color fromQColor(const QColor& c) {
    return geom::Color{ static_cast<float>(c.redF()), static_cast<float>(c.greenF()),
                         static_cast<float>(c.blueF()), static_cast<float>(c.alphaF()) };
}
} // namespace

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(4);
    tree_->setHeaderLabels({ tr("Vis"), tr("Lock"), tr("Color"), tr("Layer") });
    tree_->header()->setSectionResizeMode(kColName, QHeaderView::Stretch);
    tree_->setColumnWidth(kColVisible, 32);
    tree_->setColumnWidth(kColLocked, 36);
    tree_->setColumnWidth(kColColor, 40);
    tree_->setRootIsDecorated(false);
    layout->addWidget(tree_);

    auto* buttons = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("+ Layer"), this);
    auto* removeBtn = new QPushButton(tr("- Layer"), this);
    buttons->addWidget(addBtn);
    buttons->addWidget(removeBtn);
    layout->addLayout(buttons);

    connect(addBtn, &QPushButton::clicked, this, &LayerPanel::onAddLayerClicked);
    connect(removeBtn, &QPushButton::clicked, this, &LayerPanel::onRemoveLayerClicked);
    connect(tree_, &QTreeWidget::itemChanged, this, &LayerPanel::onItemChanged);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &LayerPanel::onItemDoubleClicked);
}

void LayerPanel::setDocument(core::Document* doc) {
    doc_ = doc;
    if (doc_) {
        doc_->layerManager().onChanged = [this] { refresh(); };
    }
    refresh();
}

void LayerPanel::refresh() {
    if (!doc_) return;
    updating_ = true;
    tree_->clear();

    for (const auto& layer : doc_->layerManager().layers()) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(kColVisible, layer.visible ? Qt::Checked : Qt::Unchecked);
        item->setCheckState(kColLocked, layer.locked ? Qt::Checked : Qt::Unchecked);

        QPixmap swatch(16, 16);
        swatch.fill(toQColor(layer.color));
        item->setIcon(kColColor, QIcon(swatch));

        item->setText(kColName, QString::fromStdString(layer.name));
        if (layer.name == "0") item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(kColName, Qt::UserRole, QString::fromStdString(layer.name));
    }
    updating_ = false;
}

void LayerPanel::onItemChanged(QTreeWidgetItem* item, int column) {
    if (updating_ || !doc_ || !item) return;
    std::string name = item->data(kColName, Qt::UserRole).toString().toStdString();

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
    if (!doc_ || !item || column != kColColor) return;
    std::string name = item->data(kColName, Qt::UserRole).toString().toStdString();
    layers::Layer* layer = doc_->layerManager().find(name);
    if (!layer) return;

    QColor chosen = QColorDialog::getColor(toQColor(layer->color), this, tr("Layer color"));
    if (chosen.isValid()) {
        layer->color = fromQColor(chosen);
        refresh();
    }
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
    std::string name = item->data(kColName, Qt::UserRole).toString().toStdString();
    doc_->layerManager().removeLayer(name);
}

} // namespace bcad::app
