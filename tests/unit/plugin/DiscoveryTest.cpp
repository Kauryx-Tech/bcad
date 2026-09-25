// Decouverte des modules : l'hote ne doit plus NOMMER un plugin (ADR-016
// principe 4). Ce test verifie la selection dans les repertoires de recherche,
// la priorite de $BCAD_PLUGIN_PATH (fichier ou dossier) et la deduplication.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo la
// macro NDEBUG est definie et assert() n'evalue pas son argument.

#include "bcad/plugin/Plugin.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace bcad::plugin;

namespace {

// Garde l'environnement propre : BCAD_PLUGIN_PATH est retabli a chaque etiquette.
class ScopedEnv {
public:
    ScopedEnv(const char* name, const std::string& value) : name_(name) {
        if (const char* previous = std::getenv(name)) {
            hadPrevious_ = true;
            previous_ = previous;
        }
        setenv(name_, value.c_str(), 1);
    }
    ~ScopedEnv() {
        if (hadPrevious_) {
            setenv(name_, previous_.c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }

private:
    const char* name_;
    bool hadPrevious_ = false;
    std::string previous_;
};

fs::path touch(const fs::path& directory, const std::string& name) {
    const fs::path file = directory / name;
    std::ofstream out(file);
    out << "x";
    return file;
}

bool contains(const std::vector<std::string>& paths, const fs::path& candidate) {
    for (const auto& path : paths) {
        if (fs::equivalent(path, candidate)) {
            return true;
        }
    }
    return false;
}

std::size_t countOf(const std::vector<std::string>& paths, const fs::path& candidate) {
    std::size_t occurrences = 0;
    for (const auto& path : paths) {
        if (fs::equivalent(path, candidate)) ++occurrences;
    }
    return occurrences;
}

fs::path makeTempRoot(const char* tag) {
    static int counter = 0;
    std::string unique = std::string(tag) + "_" + std::to_string(counter++);
#ifndef _WIN32
    unique += "_" + std::to_string(::getpid());
#endif
    const fs::path root = fs::temp_directory_path() / ("bcad_discovery_" + unique);
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

} // namespace

int main() {
    PluginManager& manager = pluginManager();

    // --- 1. Filtrage dans un repertoire de recherche ---
    const fs::path root = makeTempRoot("scan");
    const fs::path module = touch(root, "bcad_dummy_plugin"); // arbre de build : sans suffixe
    const fs::path shared = touch(root, "libbcad_dummy.so");  // module partage
    touch(root, "notes.txt");                                 // ni l'un ni l'autre
    touch(root, "libbcad_plugin.so"); // mediation de l'hote : jamais un module
    manager.addSearchDirectory(root.string());

    const auto found = manager.discoverPluginPaths();
    assert(contains(found, module));
    assert(contains(found, shared));
    assert(!contains(found, root / "notes.txt"));
    assert(!contains(found, root / "libbcad_plugin.so"));

    // --- 2. $BCAD_PLUGIN_PATH designe un fichier : prioritaire, non duplique ---
    const fs::path envRoot = makeTempRoot("envfile");
    const fs::path chosen = touch(envRoot, "bcad_env_plugin.so");
    {
        ScopedEnv env("BCAD_PLUGIN_PATH", chosen.string());
        manager.addSearchDirectory(envRoot.string());
        const auto withEnv = manager.discoverPluginPaths();
        // Le fichier designe passe en premier.
        assert(!withEnv.empty() && fs::equivalent(withEnv.front(), chosen));
        // Dedup : vu par l'env et par le scan, le module n'apparait qu'une fois.
        assert(countOf(withEnv, chosen) == 1);
    }

    // --- 3. $BCAD_PLUGIN_PATH designe un dossier : il est scanné ---
    const fs::path envDir = makeTempRoot("envdir");
    const fs::path inside = touch(envDir, "bcad_env_dir_plugin.so");
    {
        ScopedEnv env("BCAD_PLUGIN_PATH", envDir.string());
        const auto foundDir = manager.discoverPluginPaths();
        assert(contains(foundDir, inside));
    }

    // --- 4. Repertoire inexistant : aucun candidat de plus, aucune exception ---
    {
        const auto before = manager.discoverPluginPaths().size();
        manager.addSearchDirectory((root / "absent").string());
        manager.addSearchDirectory("/proc/self/nonexistent-bcad");
        assert(manager.discoverPluginPaths().size() == before);
    }

    // --- 5. loadAllDiscovered sur des fichiers non modules : echec isole ---
    {
        const fs::path junk = makeTempRoot("junk");
        const fs::path junkFile = touch(junk, "bcad_not_a_plugin");
        ScopedEnv env("BCAD_PLUGIN_PATH", junkFile.string());
        const auto loaded = manager.loadAllDiscovered();
        assert(loaded.empty()); // le module est refuse, rien ne plante
    }

    // --- 6. Ordre deterministe ---
    // L'iteration d'un repertoire n'a pas d'ordre defini, mais l'ordre de
    // chargement est celui des menus : deux decouvertes du meme repertoire
    // doivent rendre la meme sequance, triee par nom de module.
    {
        const fs::path ordered = makeTempRoot("order");
        touch(ordered, "bcad_zeta_plugin");
        touch(ordered, "bcad_alpha_plugin");
        touch(ordered, "bcad_mu_plugin");
        manager.addSearchDirectory(ordered.string());
        const auto first = manager.discoverPluginPaths();
        const auto second = manager.discoverPluginPaths();
        assert(first == second);

        std::vector<std::string> names;
        for (const auto& path : first) {
            if (path.rfind(ordered.string() + "/", 0) == 0) {
                names.push_back(fs::path(path).filename().string());
            }
        }
        assert(names.size() == 3);
        assert((names == std::vector<std::string>{"bcad_alpha_plugin", "bcad_mu_plugin",
                                                  "bcad_zeta_plugin"}));
        fs::remove_all(ordered);
    }

    fs::remove_all(root);
    fs::remove_all(envRoot);
    fs::remove_all(envDir);
    return 0;
}
