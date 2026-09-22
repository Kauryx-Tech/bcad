#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Types.h"
#include <limits>
#include <sstream>

namespace bcad::geom {

class PolylineEntity : public Entity {
public:
    PolylineEntity() = default;
    explicit PolylineEntity(std::vector<Point2> vertices, bool closed = false)
        : vertices_(std::move(vertices)), closed_(closed), properties_(std::make_unique<properties::PropertyMap>()) {}
    PolylineEntity(const PolylineEntity&) = delete;
    PolylineEntity(PolylineEntity&&) noexcept = default;
    PolylineEntity& operator=(const PolylineEntity&) = delete;
    PolylineEntity& operator=(PolylineEntity&&) = default;
    ~PolylineEntity() = default;

    EntityType type() const override { return EntityType::Polyline; }
    TypeId typeId() const override { return TypeId_Polyline; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        for (const auto& v : vertices_) bb.expand(v);
        return bb;
    }

    void applyTransform(const Transform2D& t) override {
        for (auto& v : vertices_) v = t.transform(v);
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
        std::size_t n = vertices_.size();
        if (n == 0) return best;
        if (n == 1) return distance(p, vertices_[0]);
        std::size_t segCount = closed_ ? n : n - 1;
        for (std::size_t i = 0; i < segCount; ++i) {
            const Point2& a = vertices_[i];
            const Point2& b = vertices_[(i + 1) % n];
            best = std::min(best, distance(p, closestPointOnSegment(p, a, b)));
        }
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
        std::ostringstream ss;
        ss.precision(17);
        ss << (closed_ ? 1 : 0);
        for (const auto& v : vertices_) ss << ',' << v.x_ << ',' << v.y_;
        return ss.str();
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

    bool closed() const { return closed_; }
    void setClosed(bool c) { closed_ = c; }

    // PropertyMap access
    properties::PropertyMap& properties() override { return *properties_; }
    const properties::PropertyMap& properties() const override { return *properties_; }

private:
    std::vector<Point2> vertices_;
    bool closed_ = false;
    std::unique_ptr<properties::PropertyMap> properties_;
};

} // namespace bcad::geom