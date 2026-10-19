#include "widgets/CommandLine.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace bcad::app {

namespace {

constexpr int kNameRole = Qt::UserRole + 1;
constexpr int kHistoryLines = 500;
constexpr int kRecallMax = 100;

} // namespace

CommandLine::CommandLine(QWidget* parent) : QFrame(parent) {
    setObjectName("commandBar");
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(4, 4, 6, 4);
    column->setSpacing(2);

    history_ = new QPlainTextEdit(this);
    history_->setObjectName("commandHistory");
    history_->setReadOnly(true);
    history_->setMaximumBlockCount(kHistoryLines);
    history_->setFixedHeight(140);
    history_->setFocusPolicy(Qt::NoFocus);
    history_->hide();
    column->addWidget(history_);

    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    auto* glyph = new QLabel(QStringLiteral("›_"), this);
    glyph->setObjectName("commandPrompt");
    input_ = new QLineEdit(this);
    input_->setToolTip(tr("Commande ou alias (L, CERCLE…), option, point : x,y | @dx,dy | @distance<angle — "
                          "↑ ↓ saisies précédentes, F2 historique"));
    input_->installEventFilter(this);
    row->addWidget(glyph);
    row->addWidget(input_, 1);
    column->addLayout(row);

    // Suggestions : on tape le nom, la liste montre « NOM — Libellé ».
    commands_ = new QStandardItemModel(this);
    completer_ = new QCompleter(commands_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionRole(kNameRole);
    completer_->setFilterMode(Qt::MatchStartsWith);
    completer_->setMaxVisibleItems(10);
    completer_->setWidget(input_);
    connect(input_, &QLineEdit::textChanged, this, &CommandLine::typing);
    connect(input_, &QLineEdit::textEdited, this, [this](const QString& text) {
        // Une coordonnee, une option d'une lettre en cours de commande : pas
        // de liste ; une commande de deux lettres ou plus : la liste.
        const QString token = text.trimmed();
        if (token.size() < 1 || token.front().isDigit() || token.front() == QLatin1Char('@') ||
            token.front() == QLatin1Char('-') || token.contains(QLatin1Char(','))) {
            completer_->popup()->hide();
            return;
        }
        completer_->setCompletionPrefix(token);
        if (completer_->completionCount() > 0) completer_->complete();
        else completer_->popup()->hide();
    });
    connect(completer_, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& name) {
        input_->setText(name);
        submit();
    });
    connect(input_, &QLineEdit::returnPressed, this, [this] {
        // Une suggestion choisie (surlignee) est validee par la liste ; sinon
        // Entree valide ce qui est tape, comme AutoCAD.
        QAbstractItemView* popup = completer_->popup();
        if (popup->isVisible() && popup->currentIndex().isValid()) return;
        popup->hide();
        submit();
    });
}

void CommandLine::setCommands(const QList<Command>& commands) {
    commands_->clear();
    for (const Command& command : commands) {
        auto* item = new QStandardItem(QStringLiteral("%1 — %2").arg(command.name, command.label));
        item->setData(command.name, kNameRole);
        commands_->appendRow(item);
    }
    commands_->sort(0);
}

void CommandLine::startTyping(const QString& text) {
    input_->setText(text);
    input_->setFocus();
    input_->end(false);
}

void CommandLine::setPrompt(const QString& prompt) {
    prompt_ = prompt;
    input_->setPlaceholderText(prompt);
}

void CommandLine::appendHistory(const QString& line) {
    if (!line.isEmpty()) history_->appendPlainText(line);
}

void CommandLine::toggleHistory() {
    history_->setVisible(!history_->isVisible());
    if (history_->isVisible()) history_->ensureCursorVisible();
}

void CommandLine::submit() {
    const QString text = input_->text();
    // L'historique garde la consigne et la reponse, comme AutoCAD.
    appendHistory(text.trimmed().isEmpty() ? prompt_ : QStringLiteral("%1 : %2").arg(prompt_, text.trimmed()));
    if (!text.trimmed().isEmpty() && (recalls_.isEmpty() || recalls_.back() != text)) {
        recalls_.push_back(text);
        if (recalls_.size() > kRecallMax) recalls_.removeFirst();
    }
    recallIndex_ = -1;
    input_->clear();
    emit submitted(text);
}

bool CommandLine::eventFilter(QObject* watched, QEvent* event) {
    if (watched == input_ && event->type() == QEvent::KeyPress && !completer_->popup()->isVisible()) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if ((key == Qt::Key_Up || key == Qt::Key_Down) && !recalls_.isEmpty()) {
            if (key == Qt::Key_Up)
                recallIndex_ = recallIndex_ < 0 ? recalls_.size() - 1 : std::max(0, recallIndex_ - 1);
            else
                recallIndex_ = (recallIndex_ < 0 || recallIndex_ + 1 >= recalls_.size()) ? -1 : recallIndex_ + 1;
            input_->setText(recallIndex_ < 0 ? QString() : recalls_.at(recallIndex_));
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace bcad::app
