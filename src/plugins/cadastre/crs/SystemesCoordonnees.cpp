#include "SystemesCoordonnees.h"

#include "../Templates.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cctype>

namespace bcad::cadastre {

std::vector<SystemeCoordonnees> lireSystemesCoordonnees(const std::string& cheminJson) {
    std::vector<SystemeCoordonnees> out;
    QFile file(QString::fromStdString(cheminJson));
    if (cheminJson.empty() || !file.open(QIODevice::ReadOnly)) return out;
    QJsonParseError erreur;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &erreur);
    if (erreur.error != QJsonParseError::NoError || !document.isObject()) return out;
    const QJsonObject racine = document.object();
    if (racine.value(QStringLiteral("schema_version")).toInt() != 1) return out;
    for (const QJsonValue pays : racine.value(QStringLiteral("pays")).toArray()) {
        const QString code = pays.toObject().value(QStringLiteral("code")).toString();
        for (const QJsonValue s : pays.toObject().value(QStringLiteral("systemes")).toArray()) {
            const QJsonObject o = s.toObject();
            SystemeCoordonnees sys;
            sys.epsg = o.value(QStringLiteral("epsg")).toInt();
            sys.nom = o.value(QStringLiteral("nom")).toString().toStdString();
            sys.usage = o.value(QStringLiteral("usage")).toString().toStdString();
            sys.pays = code.toStdString();
            sys.valide = o.value(QStringLiteral("statut")).toString() == QStringLiteral("valide");
            if (sys.epsg > 0 && !sys.nom.empty()) out.push_back(std::move(sys));
        }
    }
    return out;
}

std::vector<SystemeCoordonnees> systemesCoordonneesInstalles() {
    return lireSystemesCoordonnees(trouverFichierDeDonnees("cadastre/crs/systemes_coordonnees.json"));
}

std::optional<int> codeEpsgSaisi(std::string_view saisie) {
    std::string chiffres;
    std::size_t i = 0;
    while (i < saisie.size() && std::isspace(static_cast<unsigned char>(saisie[i]))) ++i;
    // Prefixe « EPSG », « EPSG: » ou « EPSG » suivi d'espaces, en toute casse.
    if (saisie.size() - i >= 4) {
        std::string tete(saisie.substr(i, 4));
        for (char& c : tete) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tete == "EPSG") {
            i += 4;
            while (i < saisie.size() && (saisie[i] == ':' || std::isspace(static_cast<unsigned char>(saisie[i])))) ++i;
        }
    }
    for (; i < saisie.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(saisie[i]);
        if (std::isdigit(c)) chiffres.push_back(static_cast<char>(c));
        else if (std::isspace(c)) break;
        else return std::nullopt;
    }
    while (i < saisie.size() && std::isspace(static_cast<unsigned char>(saisie[i]))) ++i;
    if (i != saisie.size() || chiffres.empty() || chiffres.size() > 6) return std::nullopt;
    const int code = std::stoi(chiffres);
    return code > 0 ? std::optional<int>(code) : std::nullopt;
}

std::string libelleSysteme(int epsg, const std::vector<SystemeCoordonnees>& connus) {
    for (const SystemeCoordonnees& s : connus)
        if (s.epsg == epsg) return s.nom + " (EPSG:" + std::to_string(epsg) + ")";
    return "EPSG:" + std::to_string(epsg) + " (non répertorié)";
}

} // namespace bcad::cadastre
