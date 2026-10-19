#include "widgets/OptionsDialog.h"

#include "theme/Theme.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <memory>

namespace bcad::app {

namespace {

// Un curseur et sa valeur cote a cote, comme les reglettes d'AutoCAD.
QSpinBox* sliderRow(QFormLayout* form, const QString& label, int min, int max, int value) {
    auto* row = new QWidget(form->parentWidget());
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* spin = new QSpinBox(row);
    spin->setRange(min, max);
    spin->setValue(value);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(min, max);
    slider->setValue(value);
    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    layout->addWidget(spin);
    layout->addWidget(slider, 1);
    form->addRow(label, row);
    return spin;
}

QColor crosshairColor(const std::optional<std::uint32_t>& rgb) {
    return rgb ? QColor::fromRgb(*rgb) : theme::color(theme::Token::Crosshair);
}

// --- Pages, dans l'ordre des onglets d'AutoCAD : Fichiers, Affichage, Ouvrir
// et enregistrer, Tracer et publier, Systeme, Preferences utilisateur,
// Dessin, Modelisation 3D, Selection, Profils. Une nouvelle page s'insere a
// sa place dans pagesFor().

// Affichage : le reticule (en bas a droite de l'onglet, dans AutoCAD).
OptionsPage displayPage(const detail::UserOptions& current, QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);
    QSpinBox* size = sliderRow(form, QObject::tr("Taille du réticule (%)"), detail::CursorStyle::kCrosshairMin,
                               detail::CursorStyle::kCrosshairMax, current.cursor.crosshairPercent);

    // La couleur choisie vit avec la page : partagee par ses boutons et sa relecture.
    auto color = std::make_shared<std::optional<std::uint32_t>>(current.cursor.crosshairRgb);
    auto* colorRow = new QWidget(page);
    auto* colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    auto* pick = new QPushButton(colorRow);
    auto* reset = new QPushButton(QObject::tr("Par défaut"), colorRow);
    colorLayout->addWidget(pick);
    colorLayout->addWidget(reset);
    colorLayout->addStretch();
    form->addRow(QObject::tr("Couleur du réticule"), colorRow);
    auto show = [pick, color] {
        QPixmap swatch(theme::kIconRibbonSmall, theme::kIconRibbonSmall);
        swatch.fill(crosshairColor(*color));
        pick->setIcon(QIcon(swatch));
        pick->setText(*color ? crosshairColor(*color).name() : QObject::tr("Thème"));
    };
    QObject::connect(pick, &QPushButton::clicked, page, [page, color, show] {
        const QColor chosen = QColorDialog::getColor(crosshairColor(*color), page, QObject::tr("Couleur du réticule"));
        if (!chosen.isValid()) return;
        *color = chosen.rgb() & 0xffffffu;
        show();
    });
    QObject::connect(reset, &QPushButton::clicked, page, [color, show] {
        color->reset();
        show();
    });
    show();

    auto* system = new QCheckBox(QObject::tr("Utiliser le pointeur du système (flèche) au lieu du réticule"), page);
    system->setChecked(current.cursor.systemPointer);
    form->addRow(system);

    return {QObject::tr("Affichage"), page, [size, color, system](detail::UserOptions& o) {
                o.cursor.crosshairPercent = size->value();
                o.cursor.crosshairRgb = *color;
                o.cursor.systemPointer = system->isChecked();
            }};
}

// Selection : la cible (en haut a gauche de l'onglet, dans AutoCAD).
OptionsPage selectionPage(const detail::UserOptions& current, QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);
    QSpinBox* pickbox = sliderRow(form, QObject::tr("Taille de la cible de sélection (px)"),
                                  detail::CursorStyle::kPickboxMin, detail::CursorStyle::kPickboxMax,
                                  current.cursor.pickboxPx);
    form->addRow(new QLabel(QObject::tr("La cible est aussi la distance à laquelle un clic désigne un objet."), page));
    return {QObject::tr("Sélection"), page,
            [pickbox](detail::UserOptions& o) { o.cursor.pickboxPx = pickbox->value(); }};
}

std::vector<OptionsPage> pagesFor(const detail::UserOptions& current, QWidget* parent) {
    return {displayPage(current, parent), selectionPage(current, parent)};
}

} // namespace

OptionsDialog::OptionsDialog(const detail::UserOptions& current, QWidget* parent)
    : QDialog(parent), current_(current) {
    setWindowTitle(tr("Options"));
    auto* tabs = new QTabWidget(this);
    pages_ = pagesFor(current_, tabs);
    for (const OptionsPage& page : pages_) tabs->addTab(page.widget, page.title);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Annuler"));
    buttons->button(QDialogButtonBox::Apply)->setText(tr("Appliquer"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        emit applied(options());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            [this] { emit applied(options()); });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

detail::UserOptions OptionsDialog::options() const {
    detail::UserOptions out = current_;
    for (const OptionsPage& page : pages_) page.collect(out);
    out.cursor = out.cursor.clamped();
    return out;
}

detail::UserOptions loadUserOptions() {
    QSettings s;
    detail::UserOptions o;
    o.cursor.crosshairPercent = s.value(QStringLiteral("options/crosshairPercent"), o.cursor.crosshairPercent).toInt();
    o.cursor.pickboxPx = s.value(QStringLiteral("options/pickboxPx"), o.cursor.pickboxPx).toInt();
    o.cursor.systemPointer = s.value(QStringLiteral("options/systemPointer"), false).toBool();
    if (s.contains(QStringLiteral("options/crosshairRgb")))
        o.cursor.crosshairRgb = s.value(QStringLiteral("options/crosshairRgb")).toUInt() & 0xffffffu;
    o.cursor = o.cursor.clamped();
    return o;
}

void saveUserOptions(const detail::UserOptions& o) {
    QSettings s;
    s.setValue(QStringLiteral("options/crosshairPercent"), o.cursor.crosshairPercent);
    s.setValue(QStringLiteral("options/pickboxPx"), o.cursor.pickboxPx);
    s.setValue(QStringLiteral("options/systemPointer"), o.cursor.systemPointer);
    if (o.cursor.crosshairRgb) s.setValue(QStringLiteral("options/crosshairRgb"), *o.cursor.crosshairRgb);
    else s.remove(QStringLiteral("options/crosshairRgb"));
}

} // namespace bcad::app
