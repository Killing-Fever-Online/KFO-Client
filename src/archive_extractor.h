#ifndef ARCHIVE_EXTRACTOR_H
#define ARCHIVE_EXTRACTOR_H

#include <QString>

#include <functional>

/**
 * Extracts an archive into an empty destination folder.
 *
 * The callback receives each entry path as it is processed and returns false
 * to cancel. Symlinks, hardlinks, absolute paths, and paths escaping the
 * destination are rejected.
 */
bool extractArchive(const QString &archivePath, const QString &destination,
                    QString *error,
                    const std::function<bool(const QString &)> &progress = {});

#endif // ARCHIVE_EXTRACTOR_H
