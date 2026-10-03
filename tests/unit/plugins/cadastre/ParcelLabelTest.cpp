// L'étiquette de parcelle, construite par le module : mêmes assertions que
// l'ancien test de `layout::Label::forParcel`, qui vivait dans l'API publique.
#include "layout/ParcelLabel.h"

#include <cassert>
#include <cmath>
#include <vector>

using namespace bcad::geom;

int main() {
    std::vector<Point2> square = {{0,0},{10,0},{10,10},{0,10}};
    auto label = bcad::cadastre::parcelLabel("A", "42", "100m²", square);
    assert(label.text.find("A 42") != std::string::npos);
    assert(label.text.find("100m²") != std::string::npos);
    // Centroïde d'un carré 10x10 en (0,0)-(10,10) = (5,5)
    assert(std::abs(label.position.x_ - 5) < 1e-9);
    assert(std::abs(label.position.y_ - 5) < 1e-9);

    // Triangle
    std::vector<Point2> tri = {{0,0},{6,0},{3,6}};
    auto l2 = bcad::cadastre::parcelLabel("B", "7", "", tri);
    assert(l2.text == "B 7");
    assert(std::abs(l2.position.x_ - 3) < 1e-9);

    return 0;
}
