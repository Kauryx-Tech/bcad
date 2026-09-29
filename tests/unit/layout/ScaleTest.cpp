#include "bcad/layout/Scale.h"
#include <cassert>

using namespace bcad::layout;

int main() {
    assert(scaleText(500) == "1:500");
    // La liste vient du profil du module : l'hote y choisit la premiere qui
    // tient, sans rien nommer lui-meme.
    const std::vector<int> profil{500, 1000, 2000};
    assert(nearestPermittedScale(300, profil) == 500);
    assert(nearestPermittedScale(500, profil) == 500);
    assert(nearestPermittedScale(600, profil) == 1000);
    assert(nearestPermittedScale(9999, profil) == 2000);
    // Sans liste, pas d'arrondi sur une liste que personne n'a donnee :
    // l'ajustement est exact, au plus juste.
    assert(nearestPermittedScale(300, {}) == 300);
    assert(nearestPermittedScale(299.2, {}) == 300);
    return 0;
}
