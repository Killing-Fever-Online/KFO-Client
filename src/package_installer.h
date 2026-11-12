#ifndef PACKAGE_INSTALLER_H
#define PACKAGE_INSTALLER_H

#include <QObject>
#include <QStringList>

class MountManager;
class QWidget;

/**
 * Installs dropped archive files into the packages/ folder.
 *
 * Archives are extracted into a temporary staging folder, validated, and then
 * moved into place. Existing packages prompt for Replace / Keep both / Cancel.
 */
class PackageInstaller : public QObject {
  Q_OBJECT

public:
  explicit PackageInstaller(MountManager *mounts, QObject *parent = nullptr);

  /// Installs each archive in order. Prompts use parent as their window.
  void installArchives(const QStringList &archivePaths, QWidget *parent);

  bool isBusy() const { return m_busy; }

signals:
  /// Emitted after at least one package was installed, including replacements
  /// that don't change the mount order.
  void packagesChanged();

private:
  MountManager *m_mounts = nullptr;
  bool m_busy = false;
};

#endif // PACKAGE_INSTALLER_H
