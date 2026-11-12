#ifndef PACKAGE_METADATA_H
#define PACKAGE_METADATA_H

#include <QString>

/**
 * @brief Optional metadata read from a package's package.ini.
 *
 * The icon path is relative to the package root and has already been checked
 * for path traversal and existence.
 */
struct PackageMetadata {
  bool present = false;
  QString name;
  QString author;
  QString credits;
  QString version;
  QString description;
  QString icon;
};

/// Reads package.ini from packageRoot. Returns an empty metadata value when the
/// file is missing.
PackageMetadata readPackageMetadata(const QString &packageRoot);

#endif // PACKAGE_METADATA_H
