#include "archive_drop_handler.h"

#include "package_installer.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QWidget>

ArchiveDropHandler::ArchiveDropHandler(PackageInstaller *installer,
                                       QObject *parent)
    : QObject(parent), m_installer(installer)
{
  qApp->installEventFilter(this);
}

ArchiveDropHandler::~ArchiveDropHandler()
{
  if (qApp != nullptr) {
    qApp->removeEventFilter(this);
  }
}

bool ArchiveDropHandler::isSupportedArchive(const QString &path)
{
  const QString suffix = QFileInfo(path).suffix().toLower();
  return suffix == QStringLiteral("zip") || suffix == QStringLiteral("7z") ||
         suffix == QStringLiteral("rar");
}

QStringList ArchiveDropHandler::archivesFromMimeData(
    const QMimeData *mimeData)
{
  QStringList paths;
  if (mimeData == nullptr || !mimeData->hasUrls()) {
    return paths;
  }

  const QList<QUrl> urls = mimeData->urls();
  for (const QUrl &url : urls) {
    if (!url.isLocalFile()) {
      continue;
    }
    const QString path = url.toLocalFile();
    if (isSupportedArchive(path)) {
      paths.append(path);
    }
  }
  return paths;
}

bool ArchiveDropHandler::eventFilter(QObject *watched, QEvent *event)
{
  if (m_installer == nullptr || m_installer->isBusy()) {
    return QObject::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::DragEnter:
  case QEvent::DragMove: {
    auto *dragEvent = static_cast<QDragMoveEvent *>(event);
    if (archivesFromMimeData(dragEvent->mimeData()).isEmpty()) {
      break;
    }
    dragEvent->setDropAction(Qt::CopyAction);
    dragEvent->accept();
    return true;
  }
  case QEvent::Drop: {
    auto *dropEvent = static_cast<QDropEvent *>(event);
    const QStringList archives =
        archivesFromMimeData(dropEvent->mimeData());
    if (archives.isEmpty()) {
      break;
    }

    dropEvent->setDropAction(Qt::CopyAction);
    dropEvent->accept();

    QWidget *parent = QApplication::activeWindow();
    if (parent == nullptr) {
      parent = qobject_cast<QWidget *>(watched);
    }
    m_installer->installArchives(archives, parent);
    return true;
  }
  default:
    break;
  }

  return QObject::eventFilter(watched, event);
}
