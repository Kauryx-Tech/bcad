#pragma once

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Types.h"
#include "bcad/properties/PropertyMap.h"
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace bcad::geom {

class PolylineEntity : public Entity {
public:
    PolylineEntity() = default;
    explicit PolylineEntity(std::vector<Point2> vertices, bool closed = false)
        : vertices_(std::move(vertices)), closed_(closed) {}
    PolylineEntity(const PolylineEntity&) = default;
    PolylineEntity(PolylineEntity&&) noexcept = default;
    PolylineEntity& operator=(const PolylineEntity&) = default;
    PolylineEntity& operator=(PolylineEntity&&) = default;
    ~PolylineEntity() = default;

    EntityType type() const override { return EntityType::Polyline; }
    TypeId typeId() const override { return TypeId_Polyline; }

    BoundingBox boundingBox() const override;

    void applyTransform(const Transform2D& t) override;

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<PolylineEntity>(*this);
    }

    // Le maillage suit les segments arc (D-08) : chaque segment est echantillonne
    // a la deviation demandee, comme ArcEntity.
    std::vector<Point2> tessellate(double maxDeviation) const override;

    double distanceTo(const Point2& p) const override;

    double length() const;

    std::string serializeParams() const override {
        return encodeRings(Rings{closed_, vertices_, holes_, bulges_});
    }

    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override {
        auto writeGroup = [&](int code, const std::string& value) { f << code << "\n" << value << "\n"; };
        auto writeGroupD = [&](int code, double value) { f << code << "\n" << value << "\n"; };
        
        writeGroup(0, "LWPOLYLINE");
        writeGroup(8, layer);
        if (colorOverride) {
            int r = static_cast<int>(colorOverride->r * 255);
            int g = static_cast<int>(colorOverride->g * 255);
            int b = static_cast<int>(colorOverride->b * 255);
            int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
            writeGroup(62, std::to_string(aci));
        }
        writeGroup(90, std::to_string(static_cast<int>(vertices_.size())));
        writeGroup(70, closed_ ? "1" : "0");
        for (std::size_t i = 0; i < vertices_.size(); ++i) {
            const auto& v = vertices_[i];
            writeGroupD(10, v.x_);
            writeGroupD(20, v.y_);
            // Groupe 42 (DXF §21.1.2) : bulge du segment i -> i+1, ecrit
            // seulement s'il est non nul — un segment droit ne produit rien,
            // et un lecteur ancien ignore donc la donnee sans casse.
            if (i < segmentCount()) {
                if (const double b = bulge(i); b != 0.0) writeGroupD(42, b);
            }
        }
        // LWPOLYLINE ne supporte pas les trous nativement (§0.5) : chaque
        // anneau intérieur est écrit comme une polyligne fermée séparée sur le
        // même calque — la sémantique de trou est perdue, mais les bords sont
        // visibles dans les logiciels DXF.
        for (const auto& hole : holes_) {
            if (hole.size() < 3) continue;
            writeGroup(0, "LWPOLYLINE");
            writeGroup(8, layer);
            if (colorOverride) {
                int r = static_cast<int>(colorOverride->r * 255);
                int g = static_cast<int>(colorOverride->g * 255);
                int b = static_cast<int>(colorOverride->b * 255);
                int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
                writeGroup(62, std::to_string(aci));
            }
            writeGroup(90, std::to_string(static_cast<int>(hole.size())));
            writeGroup(70, "1");
            for (const auto& v : hole) {
                writeGroupD(10, v.x_);
                writeGroupD(20, v.y_);
            }
        }
    }

    std::string geometryInfo() const override;

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override;

    // Conversion vers un polygone simple (liste de sommets dans l'ordre).
    // Pour les opérations booléennes / triangulation, voir le détail d'implémentation.
    const std::vector<Point2>& vertices() const { return vertices_; }
    std::vector<Point2>& vertices() { return vertices_; }
    void addVertex(const Point2& p) { vertices_.push_back(p); }

    // ---------------------------------------------------------- segments arc
    //
    // D-08 : un segment de polyligne peut etre un arc defini par son bulge
    // DXF (convention DXF, §21.1.2) : tan(quart de l'angle central), signe
    // positif pour un arc antihoraire du sommet i vers i+1. Le vecteur
    // « bulges_ » est par morceaux : absent = tous les segments droits, et
    // bulge(i) rend 0 au-dela de sa taille — un ancien fichier, ecrit avant
    // D-08, se lit donc sans rien changer. Seules les operations qui
    // ajoutent ou suppriment des sommets doivent le maintenir aligne
    // (GripDetail) ; celles qui deplacent des sommets le laissent tel quel.
    double bulge(std::size_t i) const { return i < bulges_.size() ? bulges_[i] : 0.0; }
    void setBulge(std::size_t i, double b) {
        if (i >= bulges_.size()) bulges_.resize(i + 1, 0.0);
        bulges_[i] = b;
    }
    const std::vector<double>& bulges() const { return bulges_; }
    std::vector<double>& bulges() { return bulges_; }
    bool hasArcs() const {
        for (double b : bulges_)
            if (b != 0.0) return true;
        return false;
    }

    // Nombre de segments geometriques (n si fermee, n-1 si ouverte).
    std::size_t segmentCount() const {
        const std::size_t n = vertices_.size();
        if (n < 2) return 0;
        return closed_ ? n : n - 1;
    }

    // Arc exact du segment i (centre, rayon, angles), nullopt si le segment
    // est droit ou degenere. Source unique pour le rendu, les distances,
    // l'aire, les boites et l'accrochage.
    std::optional<ArcEntity> segmentArc(std::size_t i) const;

    // Aire signee du contour : lacet referme sur l'anneau extieur (une
    // polyligne ouverte est supposee fermee, comme AIRE d'AutoCAD), plus la
    // correction circulaire de chaque segment arc, moins les trous (toujours
    // droits — les trous ne portent pas de bulge).
    double signedArea() const;

    // Bulge d'un arc de centre c allant de a vers b dans le sens donne
    // (antihoraire si ccw) : tan(sweep/4), le sweep ramene dans (-2pi, 2pi).
    static double bulgeBetween(const Point2& c, const Point2& a, const Point2& b, bool ccw);

    // Gestion des trous (anneaux intérieurs)
    const std::vector<std::vector<Point2>>& holes() const { return holes_; }
    std::vector<std::vector<Point2>>& holes() { return holes_; }
    void addHole(std::vector<Point2> hole) {
        if (!hole.empty()) holes_.push_back(std::move(hole));
    }
    void clearHoles() { holes_.clear(); }
    std::size_t holeCount() const { return holes_.size(); }
    bool hasHoles() const { return !holes_.empty(); }

    // ------------------------------------------------------------- grammaire
    //
    // Anneaux d'un polygone tels qu'ils voyagent dans les formats de BCAD :
    // la colonne `params` de `.bcad`, et la charge utile des entités de module
    // qui derivent de PolylineEntity. Une seule implementation, appelee par
    // l'hote et par les modules : le format s'est deja trouve recrit a quatre
    // endroits, et seule l'une des quatre copies a etre oubliee casse un
    // fichier sans le dire.
    //
    //   payload := closed , coord { , coord } [ ";" hole { ";" hole } ] [ ";" "@" bulge { "," bulge } ]
    //   coord   := x , y
    //   hole    := x , y { , x , y }
    //   bulge   := nombre   (bulge du segment i de l'anneau exterieur, D-08)
    //
    // Le trou colle a la derniere coordonnee, sans separateur superflu : le
    // « ; » tient lieu de la virgule qu'il aurait fallue.
    //
    // Ce qui precede le premier « ; » est la grammaire heritee, inchangee depuis
    // le format v1 : un payload sans « ; » se lit donc exactement comme avant
    // les trous, et un polygone sans trou s'ecrit octet pour octet comme avant
    // eux. Le « ; » ne peut naitre d'aucun nombre, ce qui tient lieu de
    // discriminant sans champ de version — ADR-015 : le versionnement est celui
    // du fichier, et le serializer tolere ce que ses versions anterieures ont
    // ecrit sans en maintenir deux en parallele.
    //
    // Les bulges suivent la meme regle : la section « @ » n'existe que si le
    // vecteur de bulges est non vide, elle est acceptee en derniere position
    // comme au milieu des trous (ordre libre — un trou ne peut commencer par
    // « @ » puisque ses champs sont des nombres stricts), et son absence rend
    // un polygone entierement droit, exactement comme avant D-08. Le nombre de
    // bulges ne peut pas depasser celui des sommets : au-dela, la charge utile
    // est mal formee et le decodeur la refuse entiere.
    struct Rings {
        bool closed = false;
        std::vector<Point2> outer;
        std::vector<std::vector<Point2>> holes;
        std::vector<double> bulges; // segment i de l'anneau exterieur (vide = tout droit)
    };

    // Ecrit la grammaire courante, et elle seule. Les trous vides sont omis :
    // un anneau sans sommet n'est pas une geometrie, et l'ecrire rendrait la
    // charge utile relisible seulement par moitie.
    static std::string encodeRings(const Rings& rings);

    // Lit la grammaire courante et toutes ses versions anterieures. Rend false
    // quand la chaine n'est pas un polygone : l'appelant doit alors conserver la
    // charge utile intacte (UnknownEntity) au lieu de deviner une geometrie.
    // La forme est seule jugee — un anneau de zero sommet est structurellement
    // correct, le seuil metier (trois sommets pour une parcelle) appartient a
    // l'appelant.
    static bool decodeRings(std::string_view payload, Rings& out);

    bool closed() const { return closed_; }
    void setClosed(bool c) { closed_ = c; }

    // PropertyMap access
    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    std::vector<Point2> vertices_;
    bool closed_ = false;
    std::vector<std::vector<Point2>> holes_;  // Anneaux intérieurs (trous)
    std::vector<double> bulges_;              // Bulges des segments extérieurs (D-08), vide = droits
    properties::PropertyMap properties_;
};

} // namespace bcad::geom