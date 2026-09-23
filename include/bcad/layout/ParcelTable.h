#pragma once

#include <string>
#include <vector>

namespace bcad::layout {

struct ParcelRow {
    std::string section, numero, contenance, commune;
    double area = 0; // m²
};

class ParcelTable {
public:
    void add(ParcelRow row) { rows_.push_back(std::move(row)); }
    const std::vector<ParcelRow>& rows() const { return rows_; }
    size_t size() const { return rows_.size(); }
    void clear() { rows_.clear(); }

    double totalArea() const {
        double t = 0;
        for (auto& r : rows_) t += r.area;
        return t;
    }

    std::string toCsv() const {
        std::string csv = "Section,Numero,Contenance,Commune,Surface(m2)\n";
        for (auto& r : rows_) {
            csv += r.section + "," + r.numero + "," + r.contenance + "," + r.commune + "," + std::to_string(r.area) + "\n";
        }
        return csv;
    }

private:
    std::vector<ParcelRow> rows_;
};

} // namespace bcad::layout
