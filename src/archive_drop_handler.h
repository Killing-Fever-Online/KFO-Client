#ifndef ARCHIVE_DROP_HANDLER_H
#define ARCHIVE_DROP_HANDLER_H

#include <QObject>
#include <QStringList>

class PackageInstaller;
class QEvent;
class QMimeData;

/**
 * Application-wide drag/drop filter that installs dropped package archives.
 */
class ArchiveDropHandler : public QObject {
  Q_OBJECT

public:
  explicit ArchiveDropHandler(PackageInstaller *installer,
                              QObject *parent = nullptr);
  ~ArchiveDropHandler() override;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  static bool isSupportedArchive(const QString &path);
  static QStringList archivesFromMimeData(const QMimeData *mimeData);

  PackageInstaller *m_installer = nullptr;
};

#endif // ARCHIVE_DROP_HANDLER_H
