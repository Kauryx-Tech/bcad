#include "Theme.h"

#include <QFile>
#include <QRegularExpression>

#include <array>

namespace bcad::app::theme {

namespace {

struct TokenDef {
    Token token;
    const char* name;
    QRgb value;  // ARGB
};

constexpr QRgb rgb(unsigned v) { return 0xff000000u | v; }

// Les valeurs, une seule fois. Ajouter un jeton : ici, dans l'enum, et dans
// docs/DESIGN_SYSTEM.md.
constexpr std::array kTokens = {
    TokenDef{Token::SurfaceSunken, "surface-sunken", rgb(0x1d2128)},
    TokenDef{Token::Field, "field", rgb(0x1f242c)},
    TokenDef{Token::Canvas, "canvas", rgb(0x212830)},
    TokenDef{Token::Window, "window", rgb(0x252a33)},
    TokenDef{Token::ScrollTrack, "scroll-track", rgb(0x262b33)},
    TokenDef{Token::Toolbar, "toolbar", rgb(0x2a3039)},
    TokenDef{Token::Caption, "caption", rgb(0x2c333e)},
    TokenDef{Token::Panel, "panel", rgb(0x2d333d)},
    TokenDef{Token::PanelAlt, "panel-alt", rgb(0x303742)},
    TokenDef{Token::Ribbon, "ribbon", rgb(0x323945)},
    TokenDef{Token::BorderPanel, "border-panel", rgb(0x363e4b)},
    TokenDef{Token::Control, "control", rgb(0x3a4250)},
    TokenDef{Token::Border, "border", rgb(0x3d4655)},
    TokenDef{Token::ControlHover, "control-hover", rgb(0x45505f)},
    TokenDef{Token::ControlRaised, "control-raised", rgb(0x4a5463)},
    TokenDef{Token::BorderHover, "border-hover", rgb(0x55617a)},
    TokenDef{Token::ControlRaisedHover, "control-raised-hover", rgb(0x5a6578)},
    TokenDef{Token::TextDisabled, "text-disabled", rgb(0x6b7380)},
    TokenDef{Token::TextCaption, "text-caption", rgb(0x9aa3af)},
    TokenDef{Token::TextSecondary, "text-secondary", rgb(0xaab3bf)},
    TokenDef{Token::TextDock, "text-dock", rgb(0xcfd6e0)},
    TokenDef{Token::TextPrimary, "text-primary", rgb(0xe3e7ec)},
    TokenDef{Token::TextStrong, "text-strong", rgb(0xffffff)},
    TokenDef{Token::Accent, "accent", rgb(0x0696d7)},
    TokenDef{Token::AccentSoft, "accent-soft", qRgba(6, 150, 215, 71)},
    TokenDef{Token::CanvasText, "canvas-text", rgb(0xebebeb)},
    TokenDef{Token::Preview, "preview", rgb(0xffc800)},
    TokenDef{Token::PreviewText, "preview-text", rgb(0xffe650)},
    TokenDef{Token::Snap, "snap", rgb(0x50dc78)},
    TokenDef{Token::GridDot, "grid-dot", rgb(0x44474c)},
    TokenDef{Token::GridAxis, "grid-axis", rgb(0x5f646a)},
    TokenDef{Token::WindowSelect, "window-select", rgb(0x508cdc)},
    TokenDef{Token::CrossingSelect, "crossing-select", rgb(0x5ac86e)},
    TokenDef{Token::Grip, "grip", rgb(0x007fff)},
    TokenDef{Token::GripHover, "grip-hover", rgb(0xff7f7f)},
    TokenDef{Token::GripHot, "grip-hot", rgb(0xe00000)},
    TokenDef{Token::Selection, "selection", rgb(0x4d9bff)},
    TokenDef{Token::SelectionGlow, "selection-glow", qRgba(77, 155, 255, 90)},
    TokenDef{Token::LayerOn, "layer-on", rgb(0xffd23c)},
    TokenDef{Token::LayerOff, "layer-off", rgb(0x5a6e8c)},
    TokenDef{Token::LockOn, "lock-on", rgb(0xe6b446)},
    TokenDef{Token::LockOff, "lock-off", rgb(0x969eaa)},
    TokenDef{Token::BulbBase, "bulb-base", rgb(0xaaafb9)},
    TokenDef{Token::SeverityError, "severity-error", rgb(0xe06c6c)},
    TokenDef{Token::SeverityWarning, "severity-warning", rgb(0xe0b060)},
    TokenDef{Token::SeverityInfo, "severity-info", rgb(0x9aa0a6)},
    TokenDef{Token::IconGeometry, "icon-geometry", rgb(0xcfd6e0)},
    TokenDef{Token::IconAction, "icon-action", rgb(0x3aa8ff)},
    TokenDef{Token::IconPoint, "icon-point", rgb(0xffc145)},
    TokenDef{Token::IconDanger, "icon-danger", rgb(0xff6b6b)},
    TokenDef{Token::IconSuccess, "icon-success", rgb(0x4fd18b)},
    TokenDef{Token::IconContrastLight, "icon-contrast-light", rgb(0xffffff)},
    TokenDef{Token::IconContrastDark, "icon-contrast-dark", rgb(0x1b1f26)},
};

// Jetons de mesure de la feuille de style, tires des constantes de Theme.h :
// une seule valeur, deux lectures (C++ et QSS).
struct MetricDef {
    const char* name;
    QString value;
};

std::array<MetricDef, 8> metrics() {
    return {{
        {"font-ui", QString::fromLatin1(kFontUi)},
        {"font-body", QStringLiteral("%1pt").arg(kFontBodyPt)},
        {"font-caption", QStringLiteral("%1pt").arg(kFontCaptionPt)},
        {"font-glyph", QStringLiteral("%1pt").arg(kFontGlyphPt)},
        {"font-mono", QString::fromLatin1(kFontMono)},
        {"radius-small", QStringLiteral("%1px").arg(kRadiusSmall)},
        {"radius", QStringLiteral("%1px").arg(kRadius)},
        {"radius-round", QStringLiteral("%1px").arg(kRadiusRound)},
    }};
}

const TokenDef* find(Token token) {
    for (const auto& def : kTokens)
        if (def.token == token) return &def;
    return nullptr;
}

QString cssValue(QRgb value) {
    const QColor c = QColor::fromRgba(value);
    if (c.alpha() == 255) return c.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 2));
}

} // namespace

QColor color(Token token) {
    const TokenDef* def = find(token);
    return def ? QColor::fromRgba(def->value) : QColor(Qt::magenta);
}

const char* name(Token token) {
    const TokenDef* def = find(token);
    return def ? def->name : "";
}

QString styleSheet() {
    QFile file(QStringLiteral(":/theme/dark.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QString sheet = QString::fromUtf8(file.readAll());
    for (const auto& def : kTokens)
        sheet.replace(QLatin1Char('@') + QLatin1String(def.name) + QLatin1Char('@'), cssValue(def.value));
    for (const auto& metric : metrics())
        sheet.replace(QLatin1Char('@') + QLatin1String(metric.name) + QLatin1Char('@'), metric.value);
    static const QRegularExpression leftover(QStringLiteral("@[a-z-]+@"));
    if (const auto m = leftover.match(sheet); m.hasMatch())
        qWarning("bcad[theme]: jeton inconnu dans dark.qss : %s", qPrintable(m.captured()));
    return sheet;
}

} // namespace bcad::app::theme
