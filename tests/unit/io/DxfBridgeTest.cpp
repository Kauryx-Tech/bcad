// Le bridge Rust <-> C++ : ce qui traverse vraiment l'ABI.
//
// `include/bcad_ffi.h` ne peut pas etre teste depuis C++ sans initialiser une
// vraie base, alors que ce qu'il faut verifier est plus simple : que la couche
// C++ lit ce que le crate promet, qu'elle ne fabrique rien, et qu'elle ne
// pretend jamais avoir reussi quand elle a echoue.
//
// Le test ecrit ses DXF en ligne plutot que dans tests/fixtures : l'objet du
// test est le bridge, et le lecteur ne devrait pas ouvrir un second fichier pour
// voir ce qui a traverse la frontiere.

#include "bcad/io/DxfBridge.h"

#include "bcad/core/Document.h"
#include "bcad/layers/LayerManager.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace bcad;

namespace {

/// A DXF with a header, a three-entry layer table and three lines.
///
/// Written inline rather than kept as a fixture: the point of the test is the
/// bridge, and the reader should not have to open a second file to see what
/// crossed the boundary.
std::string sampleDxf() {
    return "0\nSECTION\n2\nHEADER\n"
           "9\n$ACADVER\n1\nAC1015\n"
           "9\n$INSUNITS\n70\n4\n"
           "0\nENDSEC\n"
           "0\nSECTION\n2\nTABLES\n"
           "0\nTABLE\n2\nLAYER\n"
           // "0" is the document's own default layer and cannot be replaced.
           "0\nLAYER\n2\n0\n70\n0\n62\n7\n6\nCONTINUOUS\n"
           "0\nLAYER\n2\nPARCELS\n70\n0\n62\n3\n6\nCONTINUOUS\n"
           // 70 is a bitmask: 1 = frozen, 4 = locked.
           "0\nLAYER\n2\nROUTES\n70\n5\n62\n1\n6\nDASHED\n"
           "0\nENDTAB\n0\nENDSEC\n"
           "0\nSECTION\n2\nENTITIES\n"
           "0\nLINE\n8\nPARCELS\n10\n0.0\n20\n0.0\n11\n10.0\n21\n0.0\n"
           "0\nLINE\n8\nPARCELS\n10\n10.0\n20\n0.0\n11\n10.0\n21\n10.0\n"
           "0\nLINE\n8\nROUTES\n10\n0.0\n20\n0.0\n11\n0.0\n21\n10.0\n"
           "0\nENDSEC\n0\nEOF\n";
}

/// A DXF whose first CIRCLE has a radius that is not a number, followed by a
/// valid entity.
///
/// The defect is in the data, not the framing, so `Recover` keeps the file and
/// records an error-severity diagnostic: the parse *succeeds and reports a
/// problem at the same time*. A caller that only looks at `success` misses it,
/// which is why `hasErrors` exists.
std::string recoverableDefectDxf() {
    return "0\nSECTION\n2\nENTITIES\n"
           "0\nCIRCLE\n8\n0\n10\n1.0\n20\n1.0\n30\n0.0\n40\nnope\n"
           "0\nCIRCLE\n8\n0\n10\n5.0\n20\n5.0\n30\n0.0\n40\n2.0\n"
           "0\nENDSEC\n0\nEOF\n";
}

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path() / "bcad_dxf_bridge_test";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream out(path, std::ios::binary);
    out << content;
    return path;
}

std::vector<std::uint8_t> toBytes(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

// --- the layer table is really imported -----------------------------------

void importsTheLayerTable() {
    const auto path = writeTemp("layers.dxf", sampleDxf());
    const io::DxfBridgeResult result = io::readDxfMetadataFromFile(path.string());

    assert(result.success);
    assert(result.error_message.empty());
    assert(result.document != nullptr);
    assert(!io::hasErrors(result));

    assert(result.parsed_layer_count == 3);
    // "0" is skipped on purpose: LayerManager creates it and refuses to remove
    // it, so the imported colour would be the only thing that could change it.
    assert(result.imported_layer_count == 2);

    const auto& layers = result.document->layerManager().layers();
    assert(layers.size() == 3);  // "0" plus the two imported

    const layers::Layer* parcels = result.document->layerManager().find("PARCELS");
    assert(parcels != nullptr);
    // ACI 3 is the green the mirrored palette table gives.
    assert(parcels->color.g > 0.5f && parcels->color.r < 0.5f);
    assert(parcels->visible);
    assert(!parcels->locked);
    assert(parcels->lineType == layers::Layer::LineType::Continuous);

    const layers::Layer* routes = result.document->layerManager().find("ROUTES");
    assert(routes != nullptr);
    assert(routes->locked);  // 70 == 1 + 4
    assert(routes->lineType == layers::Layer::LineType::Dashed);

    // `visible` is deliberately not asserted here. The DXF spec reads group 290
    // as "0 = on, 1 = off", but bcad-dxf computes `visible = value != 0`, which
    // is the other way round, and its own test pins that reading with a 290 of 0.
    // A hidden layer therefore arrives visible. Fixing it means editing that
    // test, which AGENTS.md reserves for the maintainer, so the bridge test
    // stays out of the argument and the question is raised instead.
    // See: bcad-dxf/src/tables.rs, read_layer_record, gcode::LAYER_ON_OFF.
}

// --- the boundary is stated, not implied ----------------------------------
//
// This is the assertion that matters most. The ABI carries entity *summaries*,
// not geometry, so a Document coming back from the bridge holds a layer table
// and no entities. If someone later extends the ABI to carry geometry, this
// test is the thing that must be updated, and it says so.

void doesNotInventGeometry() {
    const auto path = writeTemp("entities.dxf", sampleDxf());
    const io::DxfBridgeResult result = io::readDxfMetadataFromFile(path.string());

    assert(result.success);
    assert(result.parsed_entity_count == 3);
    // Reported, not imported: the two numbers must never be conflated.
    assert(result.document->entities().empty());
}

// --- a recoverable defect is a diagnostic, not a silent success -----------

void reportsRecoverableDefects() {
    const auto path = writeTemp("defect.dxf", recoverableDefectDxf());
    const io::DxfBridgeResult result = io::readDxfMetadataFromFile(path.string());

    assert(result.success);  // Recover keeps going
    assert(io::hasErrors(result));
    assert(!result.diagnostics.empty());
    assert(!result.diagnostics.front().code.empty());
    assert(!result.diagnostics.front().message.empty());
}

// --- Strict turns the same file into a failure ----------------------------

void strictModeFails() {
    const auto path = writeTemp("strict.dxf", recoverableDefectDxf());
    io::DxfBridgeOptions options;
    options.recovery_mode = io::DxfRecoveryMode::Strict;

    const io::DxfBridgeResult result = io::readDxfMetadataFromFile(path.string(), options);
    assert(!result.success);
    // An empty message would leave the caller with nothing to show.
    assert(!result.error_message.empty());
    assert(result.document == nullptr);
}

// --- a failure is never silent --------------------------------------------

void reportsAMissingFile() {
    const io::DxfBridgeResult result =
        io::readDxfMetadataFromFile("/nonexistent/bcad/does-not-exist.dxf");

    assert(!result.success);
    assert(!result.error_message.empty());
    assert(result.document == nullptr);
}

void reportsGarbage() {
    const auto path = writeTemp("garbage.dxf", std::string("this is not a DXF at all\n"));
    const io::DxfBridgeResult result = io::readDxfMetadataFromFile(path.string());

    assert(!result.success);
    assert(!result.error_message.empty());
}

// --- the two entry points agree -------------------------------------------

void bytesMatchFile() {
    const std::string dxf = sampleDxf();
    const auto path = writeTemp("parity.dxf", dxf);

    const io::DxfBridgeResult fromFile = io::readDxfMetadataFromFile(path.string());
    const io::DxfBridgeResult fromBytes = io::readDxfMetadataFromBytes(toBytes(dxf));

    assert(fromFile.success && fromBytes.success);
    assert(fromFile.parsed_layer_count == fromBytes.parsed_layer_count);
    assert(fromFile.parsed_entity_count == fromBytes.parsed_entity_count);
    assert(fromFile.imported_layer_count == fromBytes.imported_layer_count);
}

void emptyBytesDoNotCrash() {
    const io::DxfBridgeResult result = io::readDxfMetadataFromBytes({});
    assert(!result.success);
    assert(!result.error_message.empty());
}

// --- the database bridge maps one to one ----------------------------------

void nullDatabaseHandlesAreSafe() {
    assert(io::databaseLayerCount(nullptr) == 0);
    assert(io::databaseEntityCount(nullptr) == 0);
    assert(io::databaseSchemaVersion(nullptr) == -1);
    io::closeDatabase(nullptr);  // must not crash
}

void missingDatabaseIsReported() {
    const io::DatabaseBridgeResult result =
        io::openDatabase("/nonexistent/bcad/no-such.db", /*readonly=*/true);
    assert(!result.success);
    assert(!result.error_message.empty());
    assert(result.handle == nullptr);
}

}  // namespace

int main() {
    importsTheLayerTable();
    doesNotInventGeometry();
    reportsRecoverableDefects();
    strictModeFails();
    reportsAMissingFile();
    reportsGarbage();
    bytesMatchFile();
    emptyBytesDoNotCrash();
    nullDatabaseHandlesAreSafe();
    missingDatabaseIsReported();
    return 0;
}
