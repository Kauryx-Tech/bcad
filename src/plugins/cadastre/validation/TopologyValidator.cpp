#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/BooleanOps.h"
#include "../entities/ParcelEntity.h"
#include <vector>
#include <string>

namespace bcad::cadastre {

struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class TopologyValidator {
public:
    ValidationResult validate(const ParcelEntity& parcel) const {
        ValidationResult r;
        if (parcel.vertices().size() < 3) {
            r.valid = false;
            r.errors.push_back("Parcelle : au moins 3 sommets requis");
        }
        if (parcelArea(parcel) < 1e-9) {
            r.valid = false;
            r.errors.push_back("Parcelle : aire nulle (sommets colinéaires)");
        }
        if (!isSimple(parcel.vertices())) {
            r.valid = false;
            r.errors.push_back("Parcelle : polygone auto-intersectant");
        }
        return r;
    }

    ValidationResult validateOverlap(const ParcelEntity& a, const ParcelEntity& b) const {
        ValidationResult r;
        // Simplified: check bounding box overlap first
        auto bbA = a.boundingBox();
        auto bbB = b.boundingBox();
        if (bbA.intersects(bbB)) {
            r.warnings.push_back("Emprise chevauchante détectée - vérification géométrique nécessaire");
        }
        return r;
    }

private:
    double parcelArea(const ParcelEntity& p) const {
        double a = 0;
        auto& v = p.vertices();
        for (size_t i = 0; i < v.size(); ++i) {
            auto& p1 = v[i];
            auto& p2 = v[(i + 1) % v.size()];
            a += (p1.x_ * p2.y_ - p2.x_ * p1.y_);
        }
        return std::abs(a * 0.5);
    }

    bool isSimple(const std::vector<geom::Point2>& v) const {
        size_t n = v.size();
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 2; j < n; ++j) {
                if (i == 0 && j == n - 1) continue;
                if (segmentsIntersect(v[i], v[(i+1)%n], v[j], v[(j+1)%n])) return false;
            }
        return true;
    }

    bool segmentsIntersect(geom::Point2 a, geom::Point2 b, geom::Point2 c, geom::Point2 d) const {
        auto cross = [](geom::Point2 o, geom::Point2 p, geom::Point2 q) {
            return (p.x_ - o.x_) * (q.y_ - o.y_) - (p.y_ - o.y_) * (q.x_ - o.x_);
        };
        auto onSeg = [](geom::Point2 p, geom::Point2 q, geom::Point2 r) {
            return q.x_ <= std::max(p.x_, r.x_) + 1e-9 && q.x_ >= std::min(p.x_, r.x_) - 1e-9 &&
                   q.y_ <= std::max(p.y_, r.y_) + 1e-9 && q.y_ >= std::min(p.y_, r.y_) - 1e-9;
        };
        auto sgn = [](double v) { return v > 1e-9 ? 1 : (v < -1e-9 ? -1 : 0); };
        int s1 = sgn(cross(c, d, a)), s2 = sgn(cross(c, d, b));
        int s3 = sgn(cross(a, b, c)), s4 = sgn(cross(a, b, d));
        if (s1 * s2 < 0 && s3 * s4 < 0) return true;
        if (s1 == 0 && onSeg(c, a, d)) return true;
        if (s2 == 0 && onSeg(c, b, d)) return true;
        if (s3 == 0 && onSeg(a, c, b)) return true;
        if (s4 == 0 && onSeg(a, d, b)) return true;
        return false;
    }
};

} // namespace bcad::cadastre