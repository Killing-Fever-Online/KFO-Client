#include "archive_extractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <archive.h>
#include <archive_entry.h>

namespace {
QString archiveError(struct archive *reader)
{
  const char *message = archive_error_string(reader);
  return message == nullptr ? QStringLiteral("Unknown archive error")
                            : QString::fromUtf8(message);
}

bool isSafeEntryPath(const QString &entryPath, QString *cleanPath)
{
  QString path = entryPath;
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));

  if (path.isEmpty() || path.startsWith(QLatin1Char('/'))) {
    return false;
  }

  // Reject Windows drive paths even when running on another platform, so a
  // package made on Windows cannot create odd paths elsewhere.
  if (path.size() >= 2 && path.at(1) == QLatin1Char(':') &&
      path.at(0).isLetter()) {
    return false;
  }

  const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  QStringList safeParts;
  for (const QString &part : parts) {
    if (part == QStringLiteral(".")) {
      continue;
    }
    if (part == QStringLiteral("..")) {
      return false;
    }
    safeParts.append(part);
  }

  if (safeParts.isEmpty()) {
    return false;
  }

  *cleanPath = QDir::cleanPath(safeParts.join(QLatin1Char('/')));
  return !cleanPath->isEmpty() && !cleanPath->startsWith(QStringLiteral(".."));
}
} // namespace

bool extractArchive(const QString &archivePath, const QString &destination,
                    QString *error,
                    const std::function<bool(const QString &)> &progress)
{
  const auto fail = [error](const QString &message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };

  QFileInfo archiveInfo(archivePath);
  if (!archiveInfo.exists() || !archiveInfo.isFile()) {
    return fail(QStringLiteral("The archive file could not be found."));
  }

  QDir destinationDir(destination);
  if (!destinationDir.exists() && !QDir().mkpath(destination)) {
    return fail(QStringLiteral("Couldn't create a temporary extraction folder."));
  }

  struct archive *reader = archive_read_new();
  if (reader == nullptr) {
    return fail(QStringLiteral("Couldn't start the archive reader."));
  }

  archive_read_support_filter_all(reader);
  archive_read_support_format_all(reader);

#ifdef Q_OS_WIN
  const int openResult = archive_read_open_filename_w(
      reader, reinterpret_cast<const wchar_t *>(archivePath.utf16()), 10240);
#else
  const QByteArray localPath = archivePath.toLocal8Bit();
  const int openResult =
      archive_read_open_filename(reader, localPath.constData(), 10240);
#endif

  if (openResult != ARCHIVE_OK) {
    const QString message = archiveError(reader);
    archive_read_free(reader);
    return fail(QStringLiteral("Couldn't open the archive: %1").arg(message));
  }

  const QString destinationRoot = QDir(destination).absolutePath();
  bool cancelled = false;

  while (true) {
    struct archive_entry *entry = nullptr;
    const int readResult = archive_read_next_header(reader, &entry);
    if (readResult == ARCHIVE_EOF) {
      break;
    }
    if (readResult != ARCHIVE_OK) {
      const QString message = archiveError(reader);
      archive_read_free(reader);
      return fail(QStringLiteral("Couldn't read the archive: %1").arg(message));
    }

    const char *rawPath = archive_entry_pathname(entry);
    if (rawPath == nullptr) {
      archive_read_free(reader);
      return fail(QStringLiteral("The archive contains an entry without a name."));
    }

    const QString entryName = QString::fromUtf8(rawPath);
    QString cleanEntry;
    if (!isSafeEntryPath(entryName, &cleanEntry)) {
      archive_read_free(reader);
      return fail(
          QStringLiteral("The archive contains an unsafe path: %1").arg(entryName));
    }

    if (progress && !progress(cleanEntry)) {
      cancelled = true;
      break;
    }

    if (archive_entry_hardlink(entry) != nullptr) {
      archive_read_free(reader);
      return fail(QStringLiteral(
          "The archive contains hardlinks, which aren't supported."));
    }

    const auto fileType = archive_entry_filetype(entry);
    if (fileType == AE_IFLNK || archive_entry_symlink(entry) != nullptr) {
      archive_read_free(reader);
      return fail(QStringLiteral(
          "The archive contains symlinks, which aren't supported."));
    }

    const QString targetPath = QDir(destinationRoot).filePath(cleanEntry);

    if (fileType == AE_IFDIR || entryName.endsWith(QLatin1Char('/'))) {
      if (!QDir().mkpath(targetPath)) {
        archive_read_free(reader);
        return fail(
            QStringLiteral("Couldn't create folder: %1").arg(cleanEntry));
      }
      continue;
    }

    if (fileType != AE_IFREG && fileType != 0) {
      archive_read_free(reader);
      return fail(QStringLiteral(
          "The archive contains an unsupported entry: %1").arg(cleanEntry));
    }

    if (QFileInfo(targetPath).isDir()) {
      archive_read_free(reader);
      return fail(
          QStringLiteral("A file conflicts with a folder: %1").arg(cleanEntry));
    }

    if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
      archive_read_free(reader);
      return fail(
          QStringLiteral("Couldn't create folder for: %1").arg(cleanEntry));
    }

    QFile output(targetPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      archive_read_free(reader);
      return fail(QStringLiteral("Couldn't write file: %1").arg(cleanEntry));
    }

    const void *buffer = nullptr;
    size_t bufferSize = 0;
    la_int64_t offset = 0;
    while (true) {
      const int dataResult =
          archive_read_data_block(reader, &buffer, &bufferSize, &offset);
      if (dataResult == ARCHIVE_EOF) {
        break;
      }
      if (dataResult != ARCHIVE_OK) {
        const QString message = archiveError(reader);
        output.close();
        archive_read_free(reader);
        return fail(
            QStringLiteral("Couldn't extract %1: %2").arg(cleanEntry, message));
      }
      if (bufferSize > 0 &&
          output.write(static_cast<const char *>(buffer),
                       static_cast<qint64>(bufferSize)) !=
              static_cast<qint64>(bufferSize)) {
        output.close();
        archive_read_free(reader);
        return fail(QStringLiteral("Couldn't write file: %1").arg(cleanEntry));
      }
    }

    output.close();
  }

  archive_read_free(reader);

  if (cancelled) {
    return fail(QStringLiteral("Installation cancelled."));
  }
  return true;
}
