#include "webcache.h"

#include "aoapplication.h"
#include "file_functions.h"
#include "options.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

WebCache::WebCache(AOApplication *parent)
    : QObject(parent)
    , ao_app(parent)
    , m_network_manager(new QNetworkAccessManager(this))
    , m_extensions_manager(new QNetworkAccessManager(this))
{
  connect(m_network_manager, &QNetworkAccessManager::finished, this, &WebCache::onDownloadFinished);
  resetDefaultExtensions();
}

WebCache::~WebCache()
{}

QString WebCache::cacheDir() const
{
  return QCoreApplication::applicationDirPath() + "/webcache/";
}

int WebCache::pendingDownloads() const
{
  return m_pending_downloads.size();
}

bool WebCache::isPending(const QString &relativePath) const
{
  const QString lowerPath = normalizeRelativePath(relativePath);
  if (lowerPath.isEmpty())
  {
    return false;
  }
  return m_pending_downloads.contains(lowerPath);
}

QString WebCache::cacheSubdir() const
{
  QString assetUrl = ao_app->asset_url;
  if (assetUrl.isEmpty())
  {
    return QString();
  }

  // Extract host and path from asset URL (e.g., "https://direct.grave.wine/base/" -> "direct.grave.wine/base/")
  QUrl url(assetUrl);
  QString subdir = url.host() + url.path();

  // Ensure it ends with /
  if (!subdir.endsWith('/'))
  {
    subdir += '/';
  }

  return subdir;
}

void WebCache::resetDefaultExtensions()
{
  m_extensions = {
      {"charicon_extensions", {".png", ".webp"}},
      {"emote_extensions", {".gif", ".png", ".apng", ".webp", ".webp.static"}},
      {"emotions_extensions", {".png", ".webp"}},
      {"background_extensions", {".png", ".gif", ".webp", ".apng"}},
      {"sfx_extensions", {".opus", ".ogg", ".mp3", ".wav"}},
      {"blip_extensions", {".opus", ".ogg", ".mp3", ".wav"}},
      {"music_extensions", {".opus", ".ogg", ".mp3", ".wav"}},
  };
}

QString WebCache::normalizeRelativePath(const QString &relativePath) const
{
  if (relativePath.isEmpty())
  {
    return QString();
  }

  // Reject absolute paths and URLs outright.
  if (relativePath.startsWith('/') || relativePath.contains("://") || relativePath.contains(":/"))
  {
    return QString();
  }

  QString cleaned = QDir::cleanPath(relativePath);
  cleaned.replace('\\', '/');

  // Reject paths that escape the cache root after normalization.
  if (cleaned.isEmpty() || cleaned == "." || cleaned == ".." || cleaned.startsWith("../"))
  {
    return QString();
  }

  return ao_app->lowercasePath(cleaned);
}

QStringList WebCache::extensions(const QString &category) const
{
  return m_extensions.value(category);
}

bool WebCache::hasServerExtensions() const
{
  return m_extensions_loaded;
}

bool WebCache::hasPendingExtensionsFetch() const
{
  return m_extensions_fetching;
}

bool WebCache::fetchExtensions()
{
  if (!Options::getInstance().webcacheEnabled())
  {
    return false;
  }

  QString assetUrl = ao_app->asset_url;
  if (assetUrl.isEmpty())
  {
    return false;
  }

  if (!assetUrl.endsWith('/'))
  {
    assetUrl += '/';
  }

  QUrl url(assetUrl + "extensions.json");
  if (!url.isValid())
  {
    return false;
  }

  // Don't re-fetch the same file repeatedly if the server sends ASS again.
  if (m_extensions_url == url.toString())
  {
    return false;
  }
  m_extensions_url = url.toString();
  // Start from built-in defaults so a partial or failed fetch from a new
  // server never inherits the previous server's advertised lists. A path that
  // failed on the previous server may well exist on the new one.
  m_extensions_loaded = false;
  m_extensions_fetching = true;
  resetDefaultExtensions();
  m_failed_downloads.clear();

  QNetworkRequest request{url};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  qDebug() << "WebCache: Fetching extensions from" << url.toString();
  QNetworkReply *reply = m_extensions_manager->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    onExtensionsFinished(reply);
  });

  // Never stall the client indefinitely on a broken asset host.
  QTimer::singleShot(5000, reply, [reply] {
    if (reply->isRunning())
    {
      qWarning() << "WebCache: extensions.json request timed out";
      reply->abort();
    }
  });

  return true;
}

void WebCache::onExtensionsFinished(QNetworkReply *reply)
{
  if (reply->error() != QNetworkReply::NoError)
  {
    qDebug() << "WebCache: Failed to fetch extensions.json -" << reply->errorString();
    m_extensions_fetching = false;
    emit extensionsFailed();
    reply->deleteLater();
    return;
  }

  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (statusCode != 200)
  {
    qDebug() << "WebCache: extensions.json returned status" << statusCode;
    m_extensions_fetching = false;
    emit extensionsFailed();
    reply->deleteLater();
    return;
  }

  QJsonParseError parse_error;
  QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject())
  {
    qWarning() << "WebCache: Invalid extensions.json:" << parse_error.errorString();
    m_extensions_fetching = false;
    emit extensionsFailed();
    reply->deleteLater();
    return;
  }

  // The server's manifest is authoritative: categories it doesn't advertise
  // must not silently fall back to the built-in extension guesses.
  m_extensions.clear();

  const QJsonObject root = document.object();
  for (auto it = root.begin(); it != root.end(); ++it)
  {
    if (!it.value().isArray())
    {
      continue;
    }

    QStringList extension_list;
    const QJsonArray array = it.value().toArray();
    for (const QJsonValue &value : array)
    {
      if (!value.isString())
      {
        continue;
      }

      QString extension = value.toString().trimmed().toLower();
      if (extension.isEmpty() || !extension.startsWith('.'))
      {
        continue;
      }
      extension_list.append(extension);
    }

    if (!extension_list.isEmpty())
    {
      m_extensions.insert(it.key(), extension_list);
    }
  }

  m_extensions_loaded = true;
  m_extensions_fetching = false;
  qDebug() << "WebCache: Loaded extensions.json with" << m_extensions.size() << "categories";
  emit extensionsLoaded();
  reply->deleteLater();
}

QString WebCache::resolveOrDownload(const QString &relativePath, const QStringList &suffixes)
{
  const QString lowerPath = resolve(relativePath, suffixes);
  if (lowerPath.isEmpty())
  {
    return QString();
  }

  download(lowerPath);
  return lowerPath;
}

QString WebCache::getCachedPath(const QString &relativePath, const QStringList &suffixes) const
{
  QString lowerPath = normalizeRelativePath(relativePath);
  if (lowerPath.isEmpty())
  {
    return QString();
  }

  QString subdir = cacheSubdir();
  if (subdir.isEmpty())
  {
    return QString();
  }

  // Try each suffix
  QStringList effectiveSuffixes = suffixes.isEmpty() ? QStringList{""} : suffixes;
  for (const QString &suffix : effectiveSuffixes)
  {
    // Use lowercase path for cache lookup (no percent-encoding in local paths)
    QString localPath = cacheDir() + subdir + lowerPath + suffix;

    if (!file_exists(localPath))
    {
      continue;
    }

    if (isExpired(localPath))
    {
      continue;
    }

    return localPath;
  }

  return QString();
}

bool WebCache::isExpired(const QString &localPath) const
{
  QFileInfo fileInfo(localPath);
  if (!fileInfo.exists())
  {
    return true;
  }

  int expiryHours = Options::getInstance().webcacheExpiryHours();
  // 0 hours means webcache never expires
  if (expiryHours <= 0)
  {
    return false;
  }
  QDateTime expiryTime = fileInfo.lastModified().addSecs(expiryHours * 3600);

  return QDateTime::currentDateTime() > expiryTime;
}

QString WebCache::resolve(const QString &relativePath, const QStringList &suffixes)
{
  // Check if webcache is enabled
  if (!Options::getInstance().webcacheEnabled())
  {
    return "";
  }

  // Check if server has an asset URL
  QString assetUrl = ao_app->asset_url;
  if (assetUrl.isEmpty())
  {
    return "";
  }

  const QString lowerBasePath = normalizeRelativePath(relativePath);
  if (lowerBasePath.isEmpty())
  {
    return "";
  }

  // Ensure asset URL ends with /
  if (!assetUrl.endsWith('/'))
  {
    assetUrl += '/';
  }

  // Get cache subdirectory for this server's asset URL
  QString subdir = cacheSubdir();
  if (subdir.isEmpty())
  {
    return "";
  }

  // Try each suffix (like webAO tries multiple extensions)
  QStringList effectiveSuffixes = suffixes.isEmpty() ? QStringList{""} : suffixes;

  // If the path already has a file extension, try the exact path first.
  const int lastSlash = lowerBasePath.lastIndexOf('/');
  if (lowerBasePath.lastIndexOf('.') > lastSlash && !effectiveSuffixes.contains(""))
  {
    effectiveSuffixes.prepend("");
  }

  for (const QString &suffix : effectiveSuffixes)
  {
    // .webp.static is a webAO marker for an unprefixed .webp file, not a
    // literal suffix. Callers that need it handle it explicitly.
    if (suffix == ".webp.static")
    {
      continue;
    }

    QString lowerPath = normalizeRelativePath(lowerBasePath + suffix);
    if (lowerPath.isEmpty())
    {
      continue;
    }

    // Check if already downloading this path
    if (m_pending_downloads.contains(lowerPath))
    {
      return "";
    }

    // Check if this path previously failed (don't retry within this session)
    if (m_failed_downloads.contains(lowerPath))
    {
      continue; // Try next suffix
    }

    // Local path uses lowercase without percent-encoding
    QString localPath = cacheDir() + subdir + lowerPath;

    // Skip if already cached and not expired
    if (file_exists(localPath) && !isExpired(localPath))
    {
      return ""; // Already have a valid cached file
    }

    // Only try one suffix at a time
    return lowerPath;
  }
  return "";
}

void WebCache::download(const QString &relativePath)
{
  // Check if server has an asset URL
  QString assetUrl = ao_app->asset_url;
  if (assetUrl.isEmpty())
  {
    return;
  }

  // Ensure asset URL ends with /
  if (!assetUrl.endsWith('/'))
  {
    assetUrl += '/';
  }

  // Get cache subdirectory for this server's asset URL
  QString subdir = cacheSubdir();
  if (subdir.isEmpty())
  {
    return;
  }

  // Lowercase and normalize the path for local storage and tracking
  QString lowerPath = normalizeRelativePath(relativePath);
  if (lowerPath.isEmpty())
  {
    return;
  }

  // Check if already downloading this path
  if (m_pending_downloads.contains(lowerPath))
  {
    return;
  }

  // Check if this path previously failed (don't retry within this session)
  if (m_failed_downloads.contains(lowerPath))
  {
    return;
  }

  // Local path uses lowercase without percent-encoding
  QString localPath = cacheDir() + subdir + lowerPath;

  // Remote URL uses percent-encoded lowercase path
  QString remoteUrl = assetUrl + ao_app->urlEncodePath(lowerPath);

  // Begin the download
  startDownload(remoteUrl, localPath, lowerPath);
}

void WebCache::startDownload(const QString &remoteUrl, const QString &localPath, const QString &relativePath)
{
  m_pending_downloads.insert(relativePath, true);
  QNetworkRequest request{QUrl(remoteUrl)};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  // Store the local path and relative path in the request for later retrieval
  request.setAttribute(QNetworkRequest::User, localPath);
  request.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1), relativePath);

  qDebug() << "WebCache: Downloading" << remoteUrl;
  m_network_manager->get(request);
}

void WebCache::onDownloadFinished(QNetworkReply *reply)
{
  QString localPath = reply->request().attribute(QNetworkRequest::User).toString();
  QString relativePath = reply->request().attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1)).toString();

  // Remove from pending
  m_pending_downloads.remove(relativePath);

  if (reply->error() != QNetworkReply::NoError)
  {
    qDebug() << "WebCache: Download failed for" << reply->url().toString(QUrl::EncodeSpaces) << "-" << reply->errorString();
    // Record the failure before notifying listeners so a listener can
    // immediately retry with the next advertised extension.
    m_failed_downloads.insert(relativePath);
    emit downloadFailed(relativePath);
    reply->deleteLater();
    return;
  }

  // Check HTTP status code
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (statusCode != 200)
  {
    qDebug() << "WebCache: Download returned status" << statusCode << "for" << reply->url().toString(QUrl::EncodeSpaces);
    // Record the failure before notifying listeners so a listener can
    // immediately retry with the next advertised extension.
    m_failed_downloads.insert(relativePath);
    emit downloadFailed(relativePath);
    reply->deleteLater();
    return;
  }

  // Create directory structure
  QFileInfo fileInfo(localPath);
  QDir dir = fileInfo.absoluteDir();
  if (!dir.exists())
  {
    if (!dir.mkpath("."))
    {
      qWarning() << "WebCache: Failed to create directory" << dir.absolutePath();
      // Record the failure before notifying listeners so a listener can
      // immediately retry with the next advertised extension.
      m_failed_downloads.insert(relativePath);
      emit downloadFailed(relativePath);
      reply->deleteLater();
      return;
    }
  }

  // Write the file
  QFile file(localPath);
  if (!file.open(QIODevice::WriteOnly))
  {
    qWarning() << "WebCache: Failed to open file for writing:" << localPath;
    // Record the failure before notifying listeners so a listener can
    // immediately retry with the next advertised extension.
    m_failed_downloads.insert(relativePath);
    emit downloadFailed(relativePath);
    reply->deleteLater();
    return;
  }

  file.write(reply->readAll());
  file.close();

  qDebug() << "WebCache: Successfully cached" << localPath;

  // Notify listeners that a file has been downloaded
  emit fileDownloaded(relativePath);

  reply->deleteLater();
}

void WebCache::clearCache()
{
  QDir dir(cacheDir());
  if (dir.exists())
  {
    if (!dir.removeRecursively())
    {
      qWarning() << "WebCache: Failed to clear cache directory";
    }
    else
    {
      qDebug() << "WebCache: Cache cleared";
    }
  }
  // Allow previously failed paths to be attempted again.
  m_failed_downloads.clear();
}

qint64 WebCache::getCacheSize() const
{
  QDir dir(cacheDir());
  if (!dir.exists())
  {
    return 0;
  }
  return calculateDirSize(dir);
}

qint64 WebCache::calculateDirSize(const QDir &dir) const
{
  qint64 size = 0;

  QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  for (const QFileInfo &fileInfo : fileList)
  {
    size += fileInfo.size();
  }

  QFileInfoList dirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &dirInfo : dirList)
  {
    size += calculateDirSize(QDir(dirInfo.absoluteFilePath()));
  }

  return size;
}
