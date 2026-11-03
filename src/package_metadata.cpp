#include "package_metadata.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {
QString readMetadataValue(const QSettings &settings, const QString &key)
{
  const QVariant grouped =
      settings.value(QStringLiteral("package/") + key);
  if (grouped.isValid()) {
    if (grouped.typeId() == QMetaType::QStringList) {
      return grouped.toStringList().join(QStringLiteral(", ")).trimmed();
    }
    return grouped.toString().trimmed();
  }

  const QVariant flat = settings.value(key);
  if (flat.typeId() == QMetaType::QStringList) {
    return flat.toStringList().join(QStringLiteral(", ")).trimmed();
  }
  return flat.toString().trimmed();
}

QString sanitizeIconPath(const QString &icon, const QString &packageRoot)
{
  QString path = icon.trimmed();
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  if (path.isEmpty() || QDir::isAbsolutePath(path)) {
    return {};
  }

  const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  QStringList safeParts;
  for (const QString &part : parts) {
    if (part == QStringLiteral(".")) {
      continue;
    }
    if (part == QStringLiteral("..")) {
      return {};
    }
    safeParts.append(part);
  }

  if (safeParts.isEmpty()) {
    return {};
  }

  const QString clean = QDir::cleanPath(safeParts.join(QLatin1Char('/')));
  if (!QFileInfo(QDir(packageRoot).filePath(clean)).isFile()) {
    return {};
  }
  return clean;
}
} // namespace

PackageMetadata readPackageMetadata(const QString &packageRoot)
{
  PackageMetadata metadata;
  const QFileInfo ini(QDir(packageRoot).filePath(QStringLiteral("package.ini")));
  if (ini.isFile()) {
    metadata.present = true;

    QSettings settings(ini.absoluteFilePath(), QSettings::IniFormat);
    metadata.name = readMetadataValue(settings, QStringLiteral("name"));
    metadata.author = readMetadataValue(settings, QStringLiteral("author"));
    metadata.credits = readMetadataValue(settings, QStringLiteral("credits"));
    metadata.version = readMetadataValue(settings, QStringLiteral("version"));
    metadata.description =
        readMetadataValue(settings, QStringLiteral("description"));
    metadata.icon = sanitizeIconPath(
        readMetadataValue(settings, QStringLiteral("icon")), packageRoot);
  }

  if (metadata.icon.isEmpty()) {
    for (const QString &candidate :
         {QStringLiteral("icon.png"), QStringLiteral("package.png")}) {
      if (QFileInfo(QDir(packageRoot).filePath(candidate)).isFile()) {
        metadata.icon = candidate;
        break;
      }
    }
  }

  return metadata;
}
