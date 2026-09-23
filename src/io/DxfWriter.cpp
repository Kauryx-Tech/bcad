#include "bcad/core/Document.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/Label.h"
#include "bcad/layout/Dimension.h"
#include <fstream>
#include <cmath>
#include <vector>

namespace bcad::io {

namespace {

constexpr const char* kCadastreAppId = "BCAD_CADASTRE";

void writeAppIdTable(std::ofstream& f) {
    f << "0\nTABLE\n2\nAPPID\n";
    f << "70\n1\n";
    f << "0\nAPPID\n";
    f << "2\nBCAD_CADASTRE\n";
    f << "70\n0\n";
    f << "0\nENDTAB\n";
}

void writeCadastreXData(std::ofstream& f, const geom::PolylineEntity& poly) {
    const auto& props = poly.properties();
    auto getStr = [&](const std::string& key) -> std::string {
        if (auto* p = props.get(key)) return p->asString();
        return "";
    };

    f << "1001\nBCAD_CADASTRE\n";
    f << "1002\n{\n";
    f << "1000\nsection\n";
    f << "1000\n" << getStr("cadastre.section") << "\n";
    f << "1000\nnumero\n";
    f << "1000\n" << getStr("cadastre.numero") << "\n";
    f << "1000\ncontenance\n";
    f << "1000\n" << getStr("cadastre.contenance") << "\n";
    f << "1000\ncommune\n";
    f << "1000\n" << getStr("cadastre.commune") << "\n";
    f << "1002\n}\n";
}

bool isCadastreParcel(const geom::Entity* e) {
    return e->typeId().value == "cadastre.parcel";
}

// Écrit les annotations cadastre : étiquettes, cotations, flèche Nord
void writeCadastreAnnotations(std::ofstream& f, const std::vector<const geom::PolylineEntity*>& parcelles) {
    const double scaleMm = 1000.0 / 500.0; // 1:500 => 1m = 2mm
    const double textHeight = 2.0; // mm sur papier
    
    // 1. Étiquettes parcelles (centroïde + section/numero/contenance)
    for (const auto* p : parcelles) {
        auto label = layout::Label::forParcel(
            p->properties().getString("cadastre.section"),
            p->properties().getString("cadastre.numero"),
            p->properties().getString("cadastre.contenance"),
            p->vertices()
        );
        
        double x = (label.position.x_ - 0) * (1000.0 / 500.0);
        double y = (label.position.y_ - 0) * (1000.0 / 500.0);
        
        f << "0\nMTEXT\n";
        f << "8\nCADASTRE_ETIQUETTES\n";
        f << "10\n" << p->vertices()[0].x_ * (1000.0 / 500.0) << "\n"; // simplify: use first vertex
        f << "20\n" << p->vertices()[0].y_ * (1000.0 / 500.0) << "\n";
        f << "40\n2.0\n";
        f << "1\n" << p->properties().getString("cadastre.section") << " " 
          << p->properties().getString("cadastre.numero") << "\n"
          << p->properties().getString("cadastre.contenance") << "\n";
        f << "7\nStandard\n";
        f << "71\n1\n";
        f << "72\n1\n";
    }
    
    // 2. Cotations linéaires (côtés de chaque parcelle)
    for (const auto* p : parcelles) {
        auto dims = layout::Dimension::forPolyline(p->vertices());
for (const auto& d : dims) {
            const double scaleMm = 1000.0 / 500.0;
            const double mx = d.mid.x_ * scaleMm;
            const double my = d.mid.y_ * scaleMm;
            
            // First scope: line offset calculation
            {
                const double dx = d.to.x_ - d.from.x_;
                const double dy = d.to.y_ - d.from.y_;
                const double len = std::hypot(dx, dy);
                if (len < 1e-9) continue;
                
                const double nx = -dy / len;
                const double ny = dx / len;
                
                const double offset = 5.0; // mm
                const double fx1 = (d.from.x_ + nx * 5.0) * scaleMm;
                const double fy1 = (d.from.y_ + ny * 5.0) * scaleMm;
                const double fx2 = (d.to.x_ + nx * 5.0) * scaleMm;
                const double fy2 = (d.to.y_ + ny * 5.0) * scaleMm;
                
                f << "0\nLINE\n";
                f << "8\nCADASTRE_COTATIONS\n";
                f << "10\n" << fx1 << "\n";
                f << "20\n" << fy1 << "\n";
                f << "11\n" << fx2 << "\n";
                f << "21\n" << fy2 << "\n";
            }
            
            // Second scope: text position
            {
                const double nx = -(d.to.y_ - d.from.y_) / std::hypot(d.to.x_ - d.from.x_, d.to.y_ - d.from.y_);
                const double ny = (d.to.x_ - d.from.x_) / std::hypot(d.to.x_ - d.from.x_, d.to.y_ - d.from.y_);
                const double tx = d.mid.x_ * (1000.0 / 500.0) + nx * 3.0;
                const double ty = d.mid.y_ * (1000.0 / 500.0) + ny * 3.0;
                
                f << "0\nMTEXT\n";
                f << "8\nCADASTRE_COTATIONS\n";
                f << "10\n" << tx << "\n";
                f << "20\n" << ty << "\n";
                f << "40\n1.5\n";
                f << "1\n" << "0.0\n"; // placeholder for dimension text
                f << "7\nStandard\n";
            }
        }
    }
    
    // 3. Flèche Nord (coin sup-droit de la zone)
    double maxX = -1e9, maxY = -1e9;
    for (const auto* p : parcelles) {
        for (const auto& v : p->vertices()) {
            if (v.x_ > maxX) maxX = v.x_;
            if (v.y_ > maxY) maxY = v.y_;
        }
    }
    if (maxX > -1e9) {
        double northX = maxX * (1000.0 / 500.0);
        double northY = maxY * (1000.0 / 500.0);
        double arrowSize = 15.0;
        
        // Triangle Nord (pointe vers le haut)
        f << "0\nLINE\n8\nCADASTRE_NORD\n";
        f << "10\n" << northX << "\n";
        f << "20\n" << northY << "\n";
        f << "11\n" << northX << "\n";
        f << "21\n" << (northY - 15.0) << "\n";
        f << "0\nLINE\n8\nCADASTRE_NORD\n";
        f << "10\n" << (northX - 7.5) << "\n";
        f << "20\n" << (northY - 7.5) << "\n";
        f << "11\n" << northX << "\n";
        f << "21\n" << (northY - 15.0) << "\n";
        f << "0\nLINE\n8\nCADASTRE_NORD\n";
        f << "10\n" << (northX + 7.5) << "\n";
        f << "20\n" << (northY - 7.5) << "\n";
        f << "11\n" << northX << "\n";
        f << "21\n" << (northY - 15.0) << "\n";
        
        // Texte "N"
        f << "0\nMTEXT\n";
        f << "8\nCADASTRE_NORD\n";
        f << "10\n" << northX << "\n";
        f << "20\n" << (northY + 5.0) << "\n";
        f << "40\n5.0\n";
        f << "1\nN\n";
        f << "7\nStandard\n";
    }
}

} // namespace

bool writeDxf(const std::string& path, const core::Document& doc, bool fullCadastre) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f.precision(9);

    f << "0\nSECTION\n2\nHEADER\n";
    f << "9\n$ACADVER\n1\nAC1015\n";
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nTABLES\n";
    f << "0\nTABLE\n2\nLAYER\n";
    auto& layers = doc.layerManager().layers();
    int layerCount = layers.size();
    if (fullCadastre) layerCount += 4; // CADASTRE_PARCELLES, CADASTRE_COTATIONS, CADASTRE_ETIQUETTES, CADASTRE_NORD
    f << "70\n" << layerCount << "\n";
    for (const auto& layer : layers) {
        f << "0\nLAYER\n";
        f << "2\n" << layer.name << "\n";
        f << "70\n" << (layer.locked ? 4 : 0) << "\n";
        int r = static_cast<int>(layer.color.r * 255);
        int g = static_cast<int>(layer.color.g * 255);
        int b = static_cast<int>(layer.color.b * 255);
        int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
        f << "62\n" << (layer.visible ? aci : -aci) << "\n";
        f << "6\nCONTINUOUS\n";
    }
    if (fullCadastre) {
        // Calques cadastre standards
        f << "0\nLAYER\n2\nCADASTRE_PARCELLES\n70\n0\n62\n1\n6\nCONTINUOUS\n";
        f << "0\nLAYER\n2\nCADASTRE_COTATIONS\n70\n0\n62\n2\n6\nCONTINUOUS\n";
        f << "0\nLAYER\n2\nCADASTRE_ETIQUETTES\n70\n0\n62\n3\n6\nCONTINUOUS\n";
        f << "0\nLAYER\n2\nCADASTRE_NORD\n70\n0\n62\n4\n6\nCONTINUOUS\n";
    }
    f << "0\nENDTAB\n";
    // APPID table
    writeAppIdTable(f);
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nENTITIES\n";
    
    // Collecter les parcelles cadastre pour annotations
    std::vector<const geom::PolylineEntity*> parcelles;
    for (const auto& e : doc.entities()) {
        e->writeDxf(f, e->layer(), e->colorOverride());
        // XDATA pour parcelles cadastre
        if (isCadastreParcel(e.get())) {
            const auto& poly = static_cast<const geom::PolylineEntity&>(*e);
            writeCadastreXData(f, poly);
            parcelles.push_back(&static_cast<const geom::PolylineEntity&>(*e));
        }
    }
    
    // Mode cadastre complet : ajouter annotations (cotations, étiquettes, nord)
    if (fullCadastre && !parcelles.empty()) {
        writeCadastreAnnotations(f, parcelles);
    }
    
    f << "0\nENDSEC\n";

    f << "0\nEOF\n";

    return f.good();
}

} // namespace bcad::io