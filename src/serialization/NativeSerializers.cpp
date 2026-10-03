#include "bcad/serialization/Serializer.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include <sstream>
#include <vector>

namespace bcad::serialization {

namespace {

// Point serializer
class PointSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Point; }
    std::string_view formatName() const override { return "BCAD Point"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& p = static_cast<const geom::PointEntity&>(entity);
        std::ostringstream ss;
        ss.precision(17);
        ss << p.position().x_ << ',' << p.position().y_;
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<double> v = parseCsv(data);
        if (v.size() < 2) return nullptr;
        return std::make_unique<geom::PointEntity>(geom::Point2(v[0], v[1]));
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }

private:
    static std::vector<double> parseCsv(const std::string& s) {
        std::vector<double> out;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, ',')) out.push_back(std::stod(token));
        return out;
    }
};

// Line serializer
class LineSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Line; }
    std::string_view formatName() const override { return "BCAD Line"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& l = static_cast<const geom::LineEntity&>(entity);
        std::ostringstream ss;
        ss.precision(17);
        ss << l.start().x_ << ',' << l.start().y_ << ',' << l.end().x_ << ',' << l.end().y_;
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<double> v = parseCsv(data);
        if (v.size() < 4) return nullptr;
        return std::make_unique<geom::LineEntity>(geom::Point2(v[0], v[1]), geom::Point2(v[2], v[3]));
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }

private:
    static std::vector<double> parseCsv(const std::string& s) {
        std::vector<double> out;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, ',')) out.push_back(std::stod(token));
        return out;
    }
};

// Circle serializer
class CircleSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Circle; }
    std::string_view formatName() const override { return "BCAD Circle"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& c = static_cast<const geom::CircleEntity&>(entity);
        std::ostringstream ss;
        ss.precision(17);
        ss << c.center().x_ << ',' << c.center().y_ << ',' << c.radius();
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<double> v = parseCsv(data);
        if (v.size() < 3) return nullptr;
        return std::make_unique<geom::CircleEntity>(geom::Point2(v[0], v[1]), v[2]);
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }

private:
    static std::vector<double> parseCsv(const std::string& s) {
        std::vector<double> out;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, ',')) out.push_back(std::stod(token));
        return out;
    }
};

// Arc serializer
class ArcSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Arc; }
    std::string_view formatName() const override { return "BCAD Arc"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& a = static_cast<const geom::ArcEntity&>(entity);
        std::ostringstream ss;
        ss.precision(17);
        ss << a.center().x_ << ',' << a.center().y_ << ',' << a.radius() << ','
           << a.startAngle() << ',' << a.endAngle();
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<double> v = parseCsv(data);
        if (v.size() < 5) return nullptr;
        return std::make_unique<geom::ArcEntity>(geom::Point2(v[0], v[1]), v[2], v[3], v[4]);
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }

private:
    static std::vector<double> parseCsv(const std::string& s) {
        std::vector<double> out;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, ',')) out.push_back(std::stod(token));
        return out;
    }
};

// Polyline serializer
class PolylineSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Polyline; }
    std::string_view formatName() const override { return "BCAD Polyline"; }

    std::string serialize(const geom::Entity& entity) const override {
        // La grammaire appartient a l'entite (cf. TextSerializer) : elle n'est
        // pas reecrite ici une seconde fois.
        return static_cast<const geom::PolylineEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        geom::PolylineEntity::Rings rings;
        if (!geom::PolylineEntity::decodeRings(data, rings)) return nullptr;
        if (rings.outer.size() < 2) return nullptr;
        auto entity = std::make_unique<geom::PolylineEntity>(std::move(rings.outer), rings.closed);
        for (auto& hole : rings.holes) entity->addHole(std::move(hole));
        return entity;
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }
};

class TextSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_Text; }
    std::string_view formatName() const override { return "BCAD Text"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::TextEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::stringstream ss(data);
        std::string token;
        std::vector<std::string> fields;
        while (std::getline(ss, token, ',')) fields.push_back(token);
        if (fields.size() < 5) return nullptr;
        try {
            return std::make_unique<geom::TextEntity>(
                geom::Point2(std::stod(fields[0]), std::stod(fields[1])),
                fields[4], std::stod(fields[2]), std::stod(fields[3]));
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }
};

namespace {

static std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) out.push_back(token);
    return out;
}

} // anonymous namespace (helpers)

class LinearDimensionSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_LinearDimension; }
    std::string_view formatName() const override { return "BCAD LinearDimension"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::LinearDimensionEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        auto f = splitCsv(data);
        if (f.size() < 8) return nullptr;
        try {
            return std::make_unique<geom::LinearDimensionEntity>(
                geom::Point2(std::stod(f[0]), std::stod(f[1])),
                geom::Point2(std::stod(f[2]), std::stod(f[3])),
                geom::Point2(std::stod(f[4]), std::stod(f[5])),
                std::stod(f[6]), f[7]);
        } catch (const std::exception&) { return nullptr; }
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line; std::getline(in, line); return deserialize(line);
    }
};

class AlignedDimensionSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_AlignedDimension; }
    std::string_view formatName() const override { return "BCAD AlignedDimension"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::AlignedDimensionEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        auto f = splitCsv(data);
        if (f.size() < 7) return nullptr;
        try {
            return std::make_unique<geom::AlignedDimensionEntity>(
                geom::Point2(std::stod(f[0]), std::stod(f[1])),
                geom::Point2(std::stod(f[2]), std::stod(f[3])),
                geom::Point2(std::stod(f[4]), std::stod(f[5])),
                f[6]);
        } catch (const std::exception&) { return nullptr; }
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line; std::getline(in, line); return deserialize(line);
    }
};

class AngularDimensionSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_AngularDimension; }
    std::string_view formatName() const override { return "BCAD AngularDimension"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::AngularDimensionEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        auto f = splitCsv(data);
        if (f.size() < 9) return nullptr;
        try {
            return std::make_unique<geom::AngularDimensionEntity>(
                geom::Point2(std::stod(f[0]), std::stod(f[1])),
                geom::Point2(std::stod(f[2]), std::stod(f[3])),
                geom::Point2(std::stod(f[4]), std::stod(f[5])),
                geom::Point2(std::stod(f[6]), std::stod(f[7])),
                f[8]);
        } catch (const std::exception&) { return nullptr; }
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line; std::getline(in, line); return deserialize(line);
    }
};

class RadiusDimensionSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_RadiusDimension; }
    std::string_view formatName() const override { return "BCAD RadiusDimension"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::RadialDimensionEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        auto f = splitCsv(data);
        if (f.size() < 8) return nullptr;
        try {
            int rt = std::stoi(f[4]);
            return std::make_unique<geom::RadialDimensionEntity>(
                geom::Point2(std::stod(f[0]), std::stod(f[1])),
                geom::Point2(std::stod(f[2]), std::stod(f[3])),
                rt == 1 ? geom::RadialDimensionEntity::RadialType::Diameter
                        : geom::RadialDimensionEntity::RadialType::Radius,
                geom::Point2(std::stod(f[5]), std::stod(f[6])),
                f[7]);
        } catch (const std::exception&) { return nullptr; }
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line; std::getline(in, line); return deserialize(line);
    }
};

class DiameterDimensionSerializer : public IEntitySerializer {
public:
    geom::TypeId typeId() const override { return geom::TypeId_DiameterDimension; }
    std::string_view formatName() const override { return "BCAD DiameterDimension"; }

    std::string serialize(const geom::Entity& entity) const override {
        return static_cast<const geom::RadialDimensionEntity&>(entity).serializeParams();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        auto f = splitCsv(data);
        if (f.size() < 8) return nullptr;
        try {
            int rt = std::stoi(f[4]);
            return std::make_unique<geom::RadialDimensionEntity>(
                geom::Point2(std::stod(f[0]), std::stod(f[1])),
                geom::Point2(std::stod(f[2]), std::stod(f[3])),
                rt == 1 ? geom::RadialDimensionEntity::RadialType::Diameter
                        : geom::RadialDimensionEntity::RadialType::Radius,
                geom::Point2(std::stod(f[5]), std::stod(f[6])),
                f[7]);
        } catch (const std::exception&) { return nullptr; }
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line; std::getline(in, line); return deserialize(line);
    }
};

} // namespace

// Enregistre tous les sérialiseurs natifs pour le format interne (SQLite)
void registerNativeSerializers() {
    SerializerRegistry::registerSerializer(std::make_unique<PointSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<LineSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<CircleSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<ArcSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<PolylineSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<TextSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<LinearDimensionSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<AlignedDimensionSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<AngularDimensionSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<RadiusDimensionSerializer>());
    SerializerRegistry::registerSerializer(std::make_unique<DiameterDimensionSerializer>());
}

// Force l'enregistrement au chargement de la bibliothèque
struct NativeSerializerRegistrar {
    NativeSerializerRegistrar() { registerNativeSerializers(); }
};
static NativeSerializerRegistrar g_nativeSerializerRegistrar;

} // namespace bcad::serialization