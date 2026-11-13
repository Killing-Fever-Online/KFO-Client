#ifndef WEBCACHE_H
#define WEBCACHE_H

#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class AOApplication;

class WebCache : public QObject
{
  Q_OBJECT

public:
  explicit WebCache(AOApplication *parent);
  ~WebCache();

signals:
  /**
   * @brief Emitted when a file has been successfully downloaded and cached.
   * @param relativePath The lowercase relative path of the cached file (e.g., "characters/phoenix/char_icon.png")
   */
  void fileDownloaded(const QString &relativePath);

  /**
   * @brief Emitted when a file has failed to download.
   * @param relativePath The lowercase relative path of the cached file (e.g., "characters/phoenix/char_icon.png")
   */
  void downloadFailed(const QString &relativePath);

  /**
   * @brief Emitted when extensions.json has been fetched and parsed.
   */
  void extensionsLoaded();

  /**
   * @brief Emitted when extensions.json could not be fetched or parsed.
   */
  void extensionsFailed();


public:
  /**
   * @brief Returns the cached file path if it exists and is not expired.
   * @param relativePath The virtual path relative to the base (e.g., "sounds/music/song.mp3")
   * @param suffixes List of file extensions to try (e.g., {".png", ".webp"})
   * @return The absolute path to the cached file, or empty string if not cached/expired.
   */
  QString getCachedPath(const QString &relativePath, const QStringList &suffixes = {""}) const;

  /**
   * @brief Check cache for the download.
   * @param relativePath The virtual path relative to the base.
   * @param suffixes List of file extensions to try (e.g., {"", ".opus", ".wav"})
   * @return Lowercase path directing to the correct suffix, empty string otherwise
   */
  QString resolve(const QString &relativePath, const QStringList &suffixes = {""});

  /**
   * @brief Helper function to call startDownload w/ all the necessary adjustments needed from just the relativePath
   */
  void download(const QString &relativePath);

  /**
   * @brief Initiates an async download for the given remote URL.
   */
  void startDownload(const QString &remoteUrl, const QString &localPath, const QString &relativePath);

  /**
   * @brief Clears the entire webcache directory.
   */
  void clearCache();

  /**
   * @brief Returns the total size of the webcache directory in bytes.
   */
  qint64 getCacheSize() const;

  /**
   * @brief Checks if a cached file has expired based on its modification time.
   * @param localPath The absolute path to the cached file.
   * @return True if the file is expired, false otherwise.
   */
  bool isExpired(const QString &localPath) const;

  /**
   * @brief Returns the webcache directory path.
   */
  QString cacheDir() const;

  /**
   * @brief Return the number of pending downloads
   */
  int pendingDownloads() const;

  /**
   * @brief Whether a download for the given relative path is in flight.
   */
  bool isPending(const QString &relativePath) const;

  /**
   * @brief Returns the cache subdirectory for the current server's asset URL.
   * E.g., "https://direct.grave.wine/base/" -> "direct.grave.wine/base/"
   */
  QString cacheSubdir() const;

  /**
   * @brief Fetch and parse extensions.json from the current asset URL.
   * @return True if a new request was started, false if no fetch is needed.
   */
  bool fetchExtensions();

  /**
   * @brief Whether an extensions.json request is currently in flight.
   * While true, remote extension probing is blocked so the manifest is
   * never bypassed by the built-in fallbacks.
   */
  bool hasPendingExtensionsFetch() const;

  /**
   * @brief Returns the advertised extension list for an asset category.
   * @param category The JSON key, e.g. "emote_extensions"
   * @return The extension list, or an empty list for an unknown category.
   */
  QStringList extensions(const QString &category) const;

  /**
   * @brief Whether a valid extensions.json was loaded from the server.
   * When true, its lists are authoritative and built-in fallbacks must not
   * be used for remote probing.
   */
  bool hasServerExtensions() const;

  /**
   * @brief Resolves a relative path and starts the download if necessary.
   * @return The normalized relative path of the started download, or an
   *         empty string if nothing needed to be downloaded.
   */
  QString resolveOrDownload(const QString &relativePath, const QStringList &suffixes = {""});

private slots:
  void onDownloadFinished(QNetworkReply *reply);
  void onExtensionsFinished(QNetworkReply *reply);

private:
  AOApplication *ao_app;
  QNetworkAccessManager *m_network_manager;
  QNetworkAccessManager *m_extensions_manager;

  QHash<QString, QStringList> m_extensions;
  QString m_extensions_url;
  bool m_extensions_loaded = false;
  bool m_extensions_fetching = false;

  // Track pending downloads to avoid duplicate requests
  QHash<QString, bool> m_pending_downloads;

  // Track failed downloads to avoid retrying them repeatedly
  QSet<QString> m_failed_downloads;

  /**
   * @brief Lowercases and cleans a relative path for cache lookup/storage.
   * @return The normalized path, or an empty string if it is invalid.
   */
  QString normalizeRelativePath(const QString &relativePath) const;

  /**
   * @brief Resets the extension lists to the built-in fallbacks.
   */
  void resetDefaultExtensions();

  /**
   * @brief Calculates directory size recursively.
   */
  qint64 calculateDirSize(const QDir &dir) const;
};

#endif // WEBCACHE_H
