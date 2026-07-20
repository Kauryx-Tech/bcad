#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Native .bcad project format: a single SQLite file with a `layers` table
// and an `entities` table (geometry packed as a compact param string).
// Chosen over a bespoke binary format so drawings stay inspectable/queryable
// with any SQLite tool, and so future features (undo log, blocks, xrefs)
// can add tables without a custom format migration story.
class Database {
public:
    static bool save(const std::string& path, const core::Document& doc);

    // Populates outDoc in place (clearing it first) rather than returning a
    // Document by value: Document holds a std::shared_mutex guarding its
    // spatial index, which makes it intentionally non-copyable/movable.
    static bool load(const std::string& path, core::Document& outDoc);
};

} // namespace bcad::io
