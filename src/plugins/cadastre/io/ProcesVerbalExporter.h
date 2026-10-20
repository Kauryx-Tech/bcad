#pragma once

#include "bcad/plugin/FileExporter.h"

#include <string>

namespace bcad::cadastre {

// Proces-verbal de bornage contradictoire en PDF (K-07), A4 portrait : le
// formulaire pre-rempli depuis le dossier, les bornes et les riverains, a
// completer et signer sur le terrain (layout/ProcesVerbal.h).
//
// Precondition : un QGuiApplication existe (QPdfWriter et polices), comme pour
// le plan cadastral.
class ProcesVerbalExporter final : public plugin::IFileExporter {
public:
    std::string id() const override { return "cadastre.pv_bornage"; }
    std::string label() const override { return "Procès-verbal de bornage (PDF)"; }
    std::string extension() const override { return "pdf"; }
    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override;
};

} // namespace bcad::cadastre
