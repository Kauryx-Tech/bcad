#include "ProcesVerbalExporter.h"

#include "layout/ProcesVerbal.h"

#include <QFile>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QString>
#include <QTextDocument>

namespace bcad::cadastre {

bool ProcesVerbalExporter::writeDocument(const core::Document& document, const std::string& path,
                                         std::string* error) const {
    const QString chemin = QString::fromStdString(path);
    {
        // QPdfWriter n'avoue pas un echec d'ouverture : on le verifie avant.
        QFile essai(chemin);
        if (!essai.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error) *error = "écriture impossible : " + path;
            return false;
        }
    }
    QPdfWriter writer(chemin);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                     QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter));
    writer.setTitle(QStringLiteral("Procès-verbal de bornage contradictoire"));
    writer.setCreator(QStringLiteral("BCAD"));

    QTextDocument texte;
    texte.setHtml(QString::fromStdString(procesVerbalHtml(buildProcesVerbal(document))));
    texte.print(&writer);
    return true;
}

} // namespace bcad::cadastre
