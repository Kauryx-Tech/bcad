// Ligne de commande, comme celle d'AutoCAD : saisie, historique des
// consignes et des resultats (F2 le deplie), rappel des saisies precedentes
// (fleches haut / bas) et suggestions de commandes pendant la frappe
// (AUTOCOMPLETE). Elle ne connait pas les outils : la fenetre lui donne la
// liste des commandes et recoit ce qui est valide.

#pragma once

#include <QFrame>
#include <QStringList>

class QCompleter;
class QLineEdit;
class QPlainTextEdit;
class QStandardItemModel;

namespace bcad::app {

class CommandLine : public QFrame {
    Q_OBJECT
public:
    explicit CommandLine(QWidget* parent = nullptr);

    // Commandes proposees : nom tape (alias ou nom complet) et libelle.
    struct Command {
        QString name;
        QString label;
    };
    void setCommands(const QList<Command>& commands);

    // Commence une saisie avec le texte deja tape dans le canevas.
    void startTyping(const QString& text);

public slots:
    // Consigne de l'etape en cours (texte d'invite du champ).
    void setPrompt(const QString& prompt);
    // Une ligne d'historique : resultat, refus, information.
    void appendHistory(const QString& line);
    // F2 : deplier / replier l'historique.
    void toggleHistory();

signals:
    void submitted(const QString& text);
    // Texte en cours de frappe (saisie dynamique pres du curseur).
    void typing(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void submit();

    QLineEdit* input_ = nullptr;
    QPlainTextEdit* history_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStandardItemModel* commands_ = nullptr;
    QStringList recalls_;   // saisies precedentes, la plus recente en dernier
    int recallIndex_ = -1;  // -1 : saisie en cours
    QString prompt_;
};

} // namespace bcad::app
