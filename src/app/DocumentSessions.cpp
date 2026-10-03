#include "DocumentSessions.h"

#include <QFileInfo>

namespace bcad::app {

namespace {

QString canonical(const QString& path) {
    const QFileInfo info(path);
    const QString resolved = info.canonicalFilePath();
    return resolved.isEmpty() ? info.absoluteFilePath() : resolved;
}

} // namespace

QString DocumentSession::displayName() const {
    return filePath.isEmpty() ? untitledName : QFileInfo(filePath).fileName();
}

QString DocumentSession::tabLabel() const {
    return dirty ? displayName() + QStringLiteral(" *") : displayName();
}

bool DocumentSession::isPristine() const {
    return filePath.isEmpty() && !dirty && document->entities().empty();
}

int DocumentSessions::addUntitled(const QString& baseName) {
    auto session = std::make_unique<DocumentSession>();
    session->untitledName = baseName + QString::number(++untitledCounter_);
    return add(std::move(session));
}

int DocumentSessions::add(std::unique_ptr<DocumentSession> session) {
    sessions_.push_back(std::move(session));
    return count() - 1;
}

void DocumentSessions::remove(int index) {
    sessions_.erase(sessions_.begin() + index);
}

void DocumentSessions::move(int from, int to) {
    auto moved = std::move(sessions_[static_cast<size_t>(from)]);
    sessions_.erase(sessions_.begin() + from);
    sessions_.insert(sessions_.begin() + to, std::move(moved));
}

int DocumentSessions::indexOfPath(const QString& path) const {
    if (path.isEmpty()) return -1;
    const QString wanted = canonical(path);
    for (int i = 0; i < count(); ++i) {
        const QString& own = sessions_[static_cast<size_t>(i)]->filePath;
        if (!own.isEmpty() && canonical(own) == wanted) return i;
    }
    return -1;
}

int DocumentSessions::indexOf(const DocumentSession* session) const {
    for (int i = 0; i < count(); ++i)
        if (sessions_[static_cast<size_t>(i)].get() == session) return i;
    return -1;
}

} // namespace bcad::app
