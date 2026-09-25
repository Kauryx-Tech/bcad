// « Feuille de plan » est la commande que l'utilisateur appelle pour obtenir le
// livrable (ADR-016) : sans ce test, elle n'est exécutée par personne et un
// PDF cassé passerait inaperçu. Elle est appelée par sa fabrique publique, le
// meme chemin que le menu du workbench.
//
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.

#include "entities/ParcelEntity.h"

#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <QGuiApplication>

namespace bcad::cadastre {
std::unique_ptr<commands::Command> makeGeneratePlanSheet(const std::vector<std::string>& args);
}

using namespace bcad;

namespace {

std::unique_ptr<cadastre::ParcelEntity> makeParcel(double x, double y,
                                                   const std::string& numero) {
    auto parcel = std::make_unique<cadastre::ParcelEntity>(std::vector<geom::Point2>{
        {x, y}, {x + 40, y}, {x + 40, y + 30}, {x, y + 30}});
    parcel->properties().setString("cadastre.section", "A");
    parcel->properties().setString("cadastre.numero", numero);
    parcel->properties().setString("cadastre.contenance", "1200,00 m²");
    parcel->properties().setString("cadastre.commune", "Lome");
    return parcel;
}

} // namespace

int main(int argc, char** argv) {
    // exportPdf() construit un QPrinter et dessine des QFont : sans
    // QGuiApplication, Qt aborte. Aucune fenetre n'est ouverte.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    const auto path = std::filesystem::temp_directory_path() / "bcad_cadastre_plan_sheet.pdf";
    std::filesystem::remove(path);

    core::Document document;
    document.addEntity(makeParcel(0, 0, "01"));
    document.addEntity(makeParcel(40, 0, "02"));

    auto command = cadastre::makeGeneratePlanSheet({path.string()});
    assert(command != nullptr);
    command->execute(document);
    assert(std::filesystem::exists(path));
    // Une feuille composee (plan, etiquettes, tableau, cartouche) ne tient pas
    // dans un fichier de quelques centaines d'octets.
    assert(std::filesystem::file_size(path) > 2000);

    // Le livrable cree par la commande est bien detruit par son annulation.
    command->undo(document);
    assert(!std::filesystem::exists(path));

    // Un document vide ne doit pas produire de fichier fantome.
    core::Document empty;
    auto nothing = cadastre::makeGeneratePlanSheet({path.string()});
    nothing->execute(empty);
    assert(!std::filesystem::exists(path));
    nothing->undo(empty);

    std::filesystem::remove(path);
    std::cout << "feuille de plan cadastral OK\n";
    return 0;
}
