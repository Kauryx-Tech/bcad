#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Types.h"
#include "bcad/properties/PropertyMap.h"
#include <limits>
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

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        for (const auto& v : vertices_) bb.expand(v);
        // Un trou valide est intérieur à l'anneau extérieur, donc ses sommets
        // sont déjà dans la boîte. On les inclut quand même : un trou mal formé
        // qui dépasse l'anneau extérieur ne passerait pas inaperçu, et le coût
        // est négligeable.
        for (const auto& hole : holes_)
            for (const auto& v : hole) bb.expand(v);
        return bb;
    }

    void applyTransform(const Transform2D& t) override {
        for (auto& v : vertices_) v = t.transform(v);
        // Les trous suivent l'anneau exterieur : une transformation qui les
        // laisserait en place decalerrait le vide du plein.
        for (auto& hole : holes_)
            for (auto& v : hole) v = t.transform(v);
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<PolylineEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override {
        std::vector<Point2> pts = vertices_;
        if (closed_ && !pts.empty()) pts.push_back(pts.front());
        return pts;
    }

    double distanceTo(const Point2& p) const override {
        double best = std::numeric_limits<double>::infinity();
        auto measureRing = [&](const std::vector<Point2>& ring, bool isClosed) {
            std::size_t n = ring.size();
            if (n == 0) return;
            if (n == 1) { best = std::min(best, distance(p, ring[0])); return; }
            std::size_t segCount = isClosed ? n : n - 1;
            for (std::size_t i = 0; i < segCount; ++i)
                best = std::min(best, distance(p, closestPointOnSegment(p, ring[i], ring[(i + 1) % n])));
        };
        measureRing(vertices_, closed_);
        // Les trous sont des anneaux toujours fermés ; accrocher un bord de trou
        // est aussi utile que d'accrocher l'anneau extérieur.
        for (const auto& hole : holes_) measureRing(hole, true);
        return best;
    }

    double length() const {
        double total = 0.0;
        std::size_t n = vertices_.size();
        if (n < 2) return 0.0;
        std::size_t segCount = closed_ ? n : n - 1;
        for (std::size_t i = 0; i < segCount; ++i) {
            total += distance(vertices_[i], vertices_[(i + 1) % n]);
        }
        return total;
    }

    std::string serializeParams() const override {
        return encodeRings(Rings{closed_, vertices_, holes_});
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
        for (const auto& v : vertices_) {
            writeGroupD(10, v.x_);
            writeGroupD(20, v.y_);
        }
    }

    std::string geometryInfo() const override;

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override;

    // Conversion vers un polygone simple (liste de sommets dans l'ordre).
    // Pour les opérations booléennes / triangulation, voir le détail d'implémentation.
    const std::vector<Point2>& vertices() const { return vertices_; }
    std::vector<Point2>& vertices() { return vertices_; }
    void addVertex(const Point2& p) { vertices_.push_back(p); }

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
    //   payload := closed , coord { , coord } [ ";" hole { ";" hole } ]
    //   coord   := x , y
    //   hole    := x , y { , x , y }
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
    struct Rings {
        bool closed = false;
        std::vector<Point2> outer;
        std::vector<std::vector<Point2>> holes;
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
    properties::PropertyMap properties_;
};

} // namespace bcad::geom