// Lecture des gabarits du module (C3). Le fichier est un donnee installee, pas
// du code : seule la valeur change, la regle reste ecrite ici.

#include "Templates.h"

#include "bcad/plugin/PluginRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cctype>
#include <filesystem>
#include <utility>
#include <vector>

namespace bcad::cadastre {

namespace {

// Version de gabarit comprise par ce module. Un gabarit d'une version
// inconnue n'est pas applique : ses cles pourraient signifier autre chose.
constexpr int kSchemaVersion = 1;

void replaceIfPresent(const QJsonObject& object, const char* key, std::string& target) {
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) return;
    const QString text = value.toString();
    if (text.isEmpty()) return;
    target = text.toStdString();
}

// Un fichier retenu remplace les valeurs qu'il donne, et seulement elles : une
// cle absente ou vide laisse la valeur par defaut du module. Une donnee mal
// ecrite ne doit pas desarmer la verification, elle doit la rendre visible dans
// les diagnostics — d'ou `source` laisse vide, que l'appelant nomme.
CadastreTemplates lireGabarit(const std::string& path, const std::string& profile) {
    CadastreTemplates templates;
    templates.profile = profile;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return templates;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return templates;

    const QJsonObject root = document.object();
    if (root.value(QLatin1String("schema_version")).toInt() != kSchemaVersion)
        return templates;

    replaceIfPresent(root.value(QLatin1String("parcel_identifier")).toObject(),
                     "section_pattern", templates.sectionPattern);
    replaceIfPresent(root.value(QLatin1String("parcel_identifier")).toObject(),
                     "number_pattern", templates.numberPattern);
    // `permitted_scales` remplace la liste par défaut entière quand il est un
    // tableau non vide d'entiers strictement positifs : une liste à moitié lue
    // ferait passer une échelle refusée pour une échelle admise.
    const QJsonValue scales = root.value(QLatin1String("permitted_scales"));
    if (scales.isArray()) {
        std::vector<int> liste;
        bool lisible = !scales.toArray().isEmpty();
        for (const QJsonValue& echelle : scales.toArray()) {
            if (!echelle.isDouble() || echelle.toInt() <= 0) {
                lisible = false;
                break;
            }
            liste.push_back(echelle.toInt());
        }
        if (lisible) templates.permittedScales = std::move(liste);
    }
    templates.source = path;
    return templates;
}

// Les repertoires proposes par l'hote. Remplis une fois pendant
// bcad_plugin_init, vides a bcad_plugin_shutdown : le module ne les ecrit
// jamais ailleurs, et ils ne vivent pas plus longtemps que le DSO.
std::vector<std::string>& repertoiresDeDonnees() {
    static std::vector<std::string> repertoires;
    return repertoires;
}

} // namespace

bool estCodeDeProfilValide(std::string_view code) {
    if (code.empty() || code.size() > 64) return false;
    // Le premier caractere doit etre une lettre : un code qui commence par un
    // point ou un chiffre n'est pas un nom de profil, c'est un reste de chemin.
    if (!std::isalpha(static_cast<unsigned char>(code.front()))) return false;
    for (const char c : code) {
        const unsigned char u = static_cast<unsigned char>(c);
        const bool autorise = std::isalnum(u) || c == '_' || c == '-' || c == '.';
        if (!autorise) return false;
    }
    // « .. » passe la regle precedente lettre par lettre : il est refuse comme
    // segment, sinon `../../secret` resterait un « nom ».
    return code.find("..") == std::string_view::npos;
}

void memoriserRepertoiresDeDonnees(const std::vector<std::string>& repertoires) {
    repertoiresDeDonnees() = repertoires;
}

std::string trouverFichierDeDonnees(const std::string& cheminRelatif) {
    namespace fs = std::filesystem;
    for (const auto& repertoire : repertoiresDeDonnees()) {
        std::error_code ec;
        const fs::path candidat = fs::path(repertoire) / cheminRelatif;
        if (fs::is_regular_file(candidat, ec)) return fs::canonical(candidat, ec).string();
    }
    return {};
}

CadastreTemplates chargerGabaritDuProfil(const std::string& codeDuProfil) {
    if (!estCodeDeProfilValide(codeDuProfil)) return CadastreTemplates{};
    namespace fs = std::filesystem;
    for (const auto& repertoire : repertoiresDeDonnees()) {
        std::error_code ec;
        const fs::path candidat = fs::path(repertoire) / ("cadastre/templates/" + codeDuProfil + ".json");
        if (!fs::is_regular_file(candidat, ec)) continue;
        return lireGabarit(fs::canonical(candidat, ec).string(), codeDuProfil);
    }
    CadastreTemplates introuvable;
    introuvable.profile = codeDuProfil;
    return introuvable;
}

CadastreTemplates loadCadastreTemplates(const plugin::PluginRegistry& registry,
                                        const std::string& profile) {
    const std::string path =
        registry.resolveDataFile("cadastre/templates/" + profile + ".json");
    if (path.empty()) {
        CadastreTemplates introuvable;
        introuvable.profile = profile;
        return introuvable;
    }
    return lireGabarit(path, profile);
}

} // namespace bcad::cadastre
