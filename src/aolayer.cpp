#include "aolayer.h"

#include "aoapplication.h"
#include "file_functions.h"
#include "misc_functions.h"
#include "networkmanager.h"
#include "options.h"
#include "webcache.h"

#include <QFileInfo>

static QThreadPool *thread_pool;

static QStringList webcache_image_extensions(AOApplication *ao_app, const QString &category)
{
  QStringList extensions = ao_app->assetExtensions(
      category, {".webp", ".apng", ".gif", ".png"});

  // .webp.static is a marker for an unprefixed .webp file, not a literal
  // extension, so translate it before probing.
  for (QString &extension : extensions)
  {
    if (extension == ".webp.static")
    {
      extension = ".webp";
    }
  }
  extensions.removeDuplicates();
  return extensions;
}

AOLayer::AOLayer(QWidget *p_parent, AOApplication *p_ao_app) : QLabel(p_parent)
{
  ao_app = p_ao_app;
  net_manager = ao_app->net_manager;

  // used for culling images when their max_duration is exceeded
  shfx_timer = new QTimer(this);
  shfx_timer->setTimerType(Qt::PreciseTimer);
  shfx_timer->setSingleShot(true);
  connect(shfx_timer, &QTimer::timeout, this, &AOLayer::shfx_timer_done);

  ticker = new QTimer(this);
  ticker->setTimerType(Qt::PreciseTimer);
  ticker->setSingleShot(false);
  connect(ticker, &QTimer::timeout, this, &AOLayer::movie_ticker);

  preanim_timer = new QTimer(this);
  preanim_timer->setSingleShot(true);
  connect(preanim_timer, &QTimer::timeout, this, &AOLayer::preanim_done);

  if (!thread_pool) {
    thread_pool = new QThreadPool(p_ao_app);
    thread_pool->setMaxThreadCount(8);
  }
}

BackgroundLayer::BackgroundLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
  connect(ao_app->webcache(), &WebCache::fileDownloaded, this, &BackgroundLayer::on_webcache_file_downloaded);
  connect(ao_app->webcache(), &WebCache::downloadFailed, this, &BackgroundLayer::on_webcache_download_failed);
}
CharLayer::CharLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
}
EffectLayer::EffectLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
}
SplashLayer::SplashLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
  connect(ao_app->webcache(), &WebCache::fileDownloaded, this, &SplashLayer::on_webcache_file_downloaded);
  connect(ao_app->webcache(), &WebCache::downloadFailed, this, &SplashLayer::on_webcache_download_failed);
}
InterfaceLayer::InterfaceLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
}
StickerLayer::StickerLayer(QWidget *p_parent, AOApplication *p_ao_app)
    : AOLayer(p_parent, p_ao_app)
{
  connect(ao_app->webcache(), &WebCache::fileDownloaded, this, &StickerLayer::on_webcache_file_downloaded);
  connect(ao_app->webcache(), &WebCache::downloadFailed, this, &StickerLayer::on_webcache_download_failed);
}

QString AOLayer::find_image(QStringList p_list)
{
  QString image_path;
  for (const QString &path : p_list) {
#ifdef DEBUG_MOVIE
    qDebug() << "checking path " << path;
#endif
    if (file_exists(path)) {
      image_path = path;
#ifdef DEBUG_MOVIE
      qDebug() << "found    path " << path;
#endif
      break;
    }
  }
  return image_path;
}

QPixmap AOLayer::get_pixmap(QImage image)
{
  QPixmap f_pixmap;
  if (m_flipped)
    f_pixmap = QPixmap::fromImage(image.mirrored(true, false));
  else
    f_pixmap = QPixmap::fromImage(image);
  //    auto aspect_ratio = Qt::KeepAspectRatio;
  if (!f_pixmap.isNull()) {
    if (f_pixmap.height() > f_h) // We are downscaling, use anti-aliasing.
      transform_mode = Qt::SmoothTransformation;
    if (stretch)
      f_pixmap = f_pixmap.scaled(f_w, f_h);
    else
      f_pixmap = f_pixmap.scaledToHeight(f_h, transform_mode);
  }
  return f_pixmap;
}

void AOLayer::set_frame(QPixmap f_pixmap)
{
  this->setPixmap(f_pixmap);
  this->center_pixmap(f_pixmap);
}

void AOLayer::center_pixmap(QPixmap f_pixmap) {
  this->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);

  QLabel::move(x, y);

  // The mask just needs to be the size of the viewport.
  // QLabel will automatically clip any parts of the image that overflow the widget.
  if (masked) {
    this->setMask(QRegion(0, 0, f_w, f_h));
  }
}

void AOLayer::combo_resize(int w, int h)
{
  QSize f_size(w, h);
  f_w = w;
  f_h = h;
  this->resize(f_size);
}

int AOLayer::get_frame_delay(int delay)
{
  return static_cast<int>(double(delay) * double(speed / 100));
}

void AOLayer::move(int ax, int ay)
{
  x = ax;
  y = ay;
  QLabel::move(x, y);
}

void AOLayer::move_and_center(int ax, int ay)
{
  x = ax;
  y = ay;
  if (movie_frames.isEmpty()) // safeguard
    QLabel::move(x,y);
  else
    center_pixmap(movie_frames[0]); // just use the first frame since dimensions are all that matter
}

bool BackgroundLayer::ensure_positions_ini(const QString &p_position, bool p_desk)
{
  const VPath ini_path = ao_app->get_background_path("positions.ini");

  // Local or previously cached positions.ini is authoritative.
  if (!ao_app->get_real_path(ini_path).isEmpty())
  {
    return false;
  }

  // Another layer may already be fetching it.
  if (ao_app->webcache()->isPending(ini_path.toQString()))
  {
    return true;
  }

  const QString lowerPath = ao_app->webcache()->resolve(ini_path.toQString());
  if (lowerPath.isEmpty())
  {
    return false;
  }

  m_pending_position = p_position;
  m_pending_desk = p_desk;
  m_pending_path = lowerPath;
  qDebug() << "WebCache: starting download of" << lowerPath;
  ao_app->webcache()->download(lowerPath);
  return true;
}

bool BackgroundLayer::download_image(QString p_position, bool p_desk)
{
  if (!Options::getInstance().webcacheEnabled() || ao_app->asset_url.isEmpty())
  {
    return false;
  }

  // Fetch positions.ini first when it isn't available locally or in cache.
  if (ensure_positions_ini(p_position, p_desk))
  {
    return true;
  }

  const QString relative_path = ao_app->get_pos_path(p_position, p_desk);
  if (relative_path.isEmpty())
  {
    return false;
  }

  const VPath vpath(relative_path);
  if (!ao_app->get_image_suffix(vpath).isEmpty())
  {
    // Already available locally or in the webcache.
    return false;
  }

  QStringList extensions;
  if (QFileInfo(relative_path).suffix().isEmpty())
  {
    // Legacy fallback for backgrounds without positions.ini.
    extensions = ao_app->assetExtensions(
        "background_extensions", {".webp", ".apng", ".gif", ".png"});
  }
  else
  {
    // positions.ini gave us the exact filename: no extension guessing.
    extensions = {""};
  }

  if (extensions.isEmpty())
  {
    return false;
  }

  const QString lowerPath = ao_app->webcache()->resolve(relative_path, extensions);
  if (lowerPath.isEmpty())
  {
    return false;
  }

  m_pending_position = p_position;
  m_pending_desk = p_desk;
  m_pending_path = lowerPath;
  qDebug() << "WebCache: starting download of background" << lowerPath;
  ao_app->webcache()->download(lowerPath);
  return true;
}

void BackgroundLayer::load_image(QString p_position, bool p_desk)
{
  play_once = false;
  cull_image = false;
  VPath design_path = ao_app->get_background_path("design.ini");
  transform_mode =
      ao_app->get_scaling(ao_app->read_design_ini("scaling", design_path));
  stretch = ao_app->read_design_ini("stretch", design_path).startsWith("true");
#ifdef DEBUG_MOVIE
  qDebug() << "[BackgroundLayer] BG loaded: " << p_position
           << " desk:" << p_desk;
#endif

  const QString relative_path = ao_app->get_pos_path(p_position, p_desk);
  QString resolved;
  if (!relative_path.isEmpty())
  {
    resolved = ao_app->get_real_path(VPath(relative_path));
    if (resolved.isEmpty())
    {
      resolved = ao_app->get_image_suffix(VPath(relative_path));
    }
  }

  if (resolved.isEmpty())
  {
    if (download_image(p_position, p_desk))
    {
      // Keep the current image while the download runs.
      return;
    }
  }

  if (resolved == last_path) {
    // Don't restart background if background is unchanged
    return;
  }

  start_playback(resolved);
  play();
}

void BackgroundLayer::on_webcache_file_downloaded(const QString &relativePath)
{
  if (relativePath != m_pending_path)
  {
    return;
  }

  const QString position = m_pending_position;
  const bool desk = m_pending_desk;
  m_pending_path.clear();
  load_image(position, desk);
}

void BackgroundLayer::on_webcache_download_failed(const QString &relativePath)
{
  if (relativePath != m_pending_path)
  {
    return;
  }

  const QString position = m_pending_position;
  const bool desk = m_pending_desk;
  m_pending_path.clear();
  // Re-resolve: this either falls back to the legacy path when positions.ini
  // failed, or lets WebCache try the next candidate.
  load_image(position, desk);
}

bool CharLayer::download_image(QString p_filename, QString p_charname, bool p_is_preanim)
{
  QString test_path = p_filename;
  // replace prefixes
  QString prefix = p_filename.left(3);
  if (prefix == "(a)" || prefix == "(b)" || prefix == "(c)")
    test_path = test_path.mid(3, -1);
  if (test_path == "-")
    test_path = "";
  if (test_path.isEmpty())
    return false;
  QVector<VPath> pathlist = ao_app->get_emote_paths(p_filename, p_charname, p_is_preanim);
  QString resolved = ao_app->get_image_path(pathlist);

  int dl_count = 0;
  // Trigger webcache download if actual character emote not found (fell back to placeholder or not found)
  if ((resolved.isEmpty() || resolved.contains("/placeholder")) && Options::getInstance().webcacheEnabled())
  {
    QStringList image_suffixes = ao_app->assetExtensions(
        "emote_extensions", {".webp", ".apng", ".gif", ".png"});
    // .webp.static is a marker for an unprefixed .webp file, not a literal
    // extension. WebCache skips it, so translate it here and let the pathlist
    // below include the unprefixed candidate.
    for (QString &suffix : image_suffixes)
    {
      if (suffix == ".webp.static")
      {
        suffix = ".webp";
      }
    }
    image_suffixes.removeDuplicates();

    // Try downloading the character-specific paths (first 4 entries, before
    // placeholder fallbacks). An empty list means the server didn't advertise
    // emote extensions, so don't guess.
    if (!image_suffixes.isEmpty())
    {
      for (int i = 0; i < 4 && i < pathlist.size(); ++i)
      {
        QString path = pathlist[i].toQString();
        if (!path.isEmpty())
        {
          QString lowerPath = ao_app->webcache()->resolve(path, image_suffixes);
          if (!lowerPath.isEmpty())
          {
            qDebug() << "Resolving " << path << " for " << p_filename << " , " << p_charname << " , preanim: " << p_is_preanim;
            dl_count += 1;
            ao_app->webcache()->download(lowerPath);
          }
        }
      }
    }
  }
  return dl_count > 0;
}

void CharLayer::load_image(QString p_filename, QString p_charname,
                           int p_duration, bool p_is_preanim)
{
  duration = p_duration;
  cull_image = false;
  force_continuous = false;
  transform_mode = ao_app->get_scaling(
      ao_app->get_emote_property(p_charname, p_filename, "scaling"));
  stretch = ao_app->get_emote_property(p_charname, p_filename, "stretch")
                .startsWith("true");
  if ((p_charname == last_char) &&
      ((p_filename == last_emote) ||
       (p_filename.mid(3, -1) == last_emote.mid(3, -1))) &&
      (!is_preanim) && (!was_preanim)) {
    continuous = true;
    force_continuous = true;
  }
  else {
    continuous = false;
    force_continuous = true;
  }
  prefix = "";
  current_emote = p_filename;
  was_preanim = is_preanim;
  m_char = p_charname;
  m_emote = current_emote;
  last_char = p_charname;
  last_emote = current_emote;
  last_prefix = prefix;
  is_preanim = p_is_preanim;
  if ((p_filename.left(3) == "(a)") || (p_filename.left(3) == "(b)")) { // if we are playing an idle or talking animation
    prefix = p_filename.left(3); // separate the prefix from the emote name
    current_emote = p_filename.mid(3, -1);
  }
  else if ((duration > 0) || (p_filename.left(3) == "(c)")) { // else if we are playing a preanim or postanim
    if (p_filename.left(3) == "(c)") { // if we are playing a postanim
      prefix = "(c)"; // separate the prefix from the emote name
      current_emote = p_filename.mid(3, -1);
    }
    // pre/postanim specific flags
    is_preanim = true;
    play_once = true;
    preanim_timer->start(duration);
  }
#ifdef DEBUG_MOVIE
  qDebug() << "[CharLayer] anim loaded: prefix " << prefix << " filename "
           << current_emote << " from character: " << p_charname
           << " continuous: " << continuous;
#endif
  QVector<VPath> pathlist = ao_app->get_emote_paths(p_filename, p_charname, p_is_preanim);
  QString resolved = ao_app->get_image_path(pathlist);

  start_playback(resolved);
  play();
}

bool SplashLayer::download_image(QString p_filename, QString p_charname,
                                 QString p_miscname, bool p_reload_on_finish)
{
  QString final_image = ao_app->get_image(p_filename,
      Options::getInstance().theme(), Options::getInstance().subTheme(),
      ao_app->default_theme, p_miscname, p_charname, "placeholder");

  // Trigger a webcache download if the requested splash image is missing or
  // only resolved to a placeholder.
  if ((final_image.isEmpty() || final_image.contains("/placeholder")) &&
      Options::getInstance().webcacheEnabled())
  {
    m_pending_filename = p_filename;
    m_pending_charname = p_charname;
    m_pending_miscname = p_miscname;
    m_reload_on_finish = p_reload_on_finish;

    QVector<VPath> pathlist = ao_app->get_asset_paths(p_filename,
        Options::getInstance().theme(), Options::getInstance().subTheme(),
        ao_app->default_theme, p_miscname, p_charname, "placeholder");
    const QStringList extensions = webcache_image_extensions(ao_app, "emote_extensions");

    // An empty list means the server manifest didn't advertise emote
    // extensions, so don't guess and spam the server.
    if (!extensions.isEmpty())
    {
      for (const VPath &path : pathlist)
      {
        if (path.toQString().contains("placeholder"))
        {
          continue;
        }

        QString lowerPath = ao_app->webcache()->resolve(path.toQString(), extensions);
        if (!lowerPath.isEmpty())
        {
          qDebug() << "WebCache: starting download of splash image" << lowerPath;
          m_pending_path = lowerPath;
          ao_app->webcache()->download(lowerPath);
          return true;
        }
      }
    }
  }

  return false;
}

void SplashLayer::load_image(QString p_filename, QString p_charname,
                                   QString p_miscname)
{
  transform_mode = ao_app->get_misc_scaling(p_miscname);

  if (download_image(p_filename, p_charname, p_miscname))
  {
    return;
  }

  QString final_image = ao_app->get_image(p_filename, Options::getInstance().theme(), Options::getInstance().subTheme(), ao_app->default_theme, p_miscname, p_charname, "placeholder");
  start_playback(final_image);
  play();
}

void SplashLayer::on_webcache_file_downloaded(const QString &relativePath)
{
  if (relativePath != m_pending_path || m_pending_path.isEmpty())
  {
    return;
  }
  m_pending_path.clear();
  if (!m_reload_on_finish)
  {
    return;
  }
  load_image(m_pending_filename, m_pending_charname, m_pending_miscname);
}

void SplashLayer::on_webcache_download_failed(const QString &relativePath)
{
  if (relativePath != m_pending_path || m_pending_path.isEmpty())
  {
    return;
  }
  m_pending_path.clear();
  // Try the next candidate/extension even when prefetching, but only show the
  // image if this layer is the one that requested it.
  const bool retrying = download_image(m_pending_filename, m_pending_charname,
                                       m_pending_miscname, m_reload_on_finish);
  if (!retrying && m_reload_on_finish)
  {
    // Every candidate failed; fall back to whatever get_image() resolves to.
    load_image(m_pending_filename, m_pending_charname, m_pending_miscname);
  }
}

void EffectLayer::load_image(QString p_filename, bool p_looping)
{
  if (p_looping)
    play_once = false;
  else
    play_once = true;
  continuous = false;
  force_continuous = true;
  cull_image = false;

  start_playback(p_filename); // path resolution is handled by the caller for EffectLayer objects
  play();
}

void InterfaceLayer::load_image(QString p_filename, QString p_miscname)
{
  last_path = "";
  stretch = true;
  QString final_image = ao_app->get_image(p_filename, Options::getInstance().theme(), Options::getInstance().subTheme(), ao_app->default_theme, p_miscname);
  start_playback(final_image);
  play();
}

void StickerLayer::load_image(QString p_charname)
{
  QString p_miscname;
  if (Options::getInstance().customChatboxEnabled())
    p_miscname = ao_app->get_chat(p_charname);
  transform_mode = ao_app->get_misc_scaling(p_miscname);
  QString final_image = ao_app->get_image("sticker/" + p_charname, Options::getInstance().theme(), Options::getInstance().subTheme(), ao_app->default_theme, p_miscname);

  if ((final_image.isEmpty() || final_image.contains("/placeholder")) &&
      Options::getInstance().webcacheEnabled())
  {
    m_pending_charname = p_charname;

    QVector<VPath> pathlist = ao_app->get_asset_paths("sticker/" + p_charname,
        Options::getInstance().theme(), Options::getInstance().subTheme(),
        ao_app->default_theme, p_miscname, "", "placeholder");
    const QStringList extensions = webcache_image_extensions(ao_app, "emote_extensions");

    // An empty list means the server manifest didn't advertise emote
    // extensions, so don't guess and spam the server.
    if (!extensions.isEmpty())
    {
      for (const VPath &path : pathlist)
      {
        if (path.toQString().contains("placeholder"))
        {
          continue;
        }

        QString lowerPath = ao_app->webcache()->resolve(path.toQString(), extensions);
        if (!lowerPath.isEmpty())
        {
          qDebug() << "WebCache: starting download of sticker image" << lowerPath;
          m_pending_path = lowerPath;
          ao_app->webcache()->download(lowerPath);
          return;
        }
      }
    }
  }

  start_playback(final_image);
  play();
}

void StickerLayer::on_webcache_file_downloaded(const QString &relativePath)
{
  if (relativePath != m_pending_path || m_pending_path.isEmpty())
  {
    return;
  }
  m_pending_path.clear();
  load_image(m_pending_charname);
}

void StickerLayer::on_webcache_download_failed(const QString &relativePath)
{
  if (relativePath != m_pending_path || m_pending_path.isEmpty())
  {
    return;
  }
  m_pending_path.clear();
  load_image(m_pending_charname);
}

void CharLayer::start_playback(QString p_image)
{
  movie_effects.clear();
  AOLayer::start_playback(p_image);
  if (network_strings.size() > 0) // our FX overwritten by networked ones
    load_network_effects();
  else // Use default ini FX
    load_effects();
  play();
}

void AOLayer::start_playback(QString p_image)
{
  if (p_image == "") {// image wasn't found by the path resolution function
    this->kill();
    return;
  }
  
  if (p_image.startsWith("http") && Options::getInstance().assetStreaming()) {
    disconnect(net_manager, &NetworkManager::imageLoaded, this, &AOLayer::onImageLoaded);
    // qDebug() << "Loading... " << p_image;
    ao_app->net_manager->start_image_streaming(p_image, ".png");
    // qDebug() << "Started image streaming: " << p_image;
    connect(net_manager, &NetworkManager::imageLoaded, this, &AOLayer::onImageLoaded);
    p_image = ao_app->get_real_path(ao_app->get_misc_path("default", "loading"));
  }

  if (frame_loader.isRunning())
    exit_loop = true; // tell the loader to stop, we have a new image to load

  QMutexLocker locker(&mutex);
  this->show();

  if (!Options::getInstance().continuousPlaybackEnabled()) {
    continuous = false;
    force_continuous = true;
  }

  if (((last_path == p_image) && (!force_continuous)) || p_image.isEmpty())
    return;

#ifdef DEBUG_MOVIE
  actual_time.restart();
#endif
  this->clear();
  this->freeze();
  movie_frames.clear();
  movie_delays.clear();
  QString scaling_override =
      ao_app->read_design_ini("scaling", p_image + ".ini");
  if (scaling_override != "")
    transform_mode = ao_app->get_scaling(scaling_override);
  QString stretch_override =
      ao_app->read_design_ini("stretch", p_image + ".ini");
  if (stretch_override != "")
    stretch = stretch_override.startsWith("true");

#ifdef DEBUG_MOVIE
  qDebug() << "[AOLayer::start_playback] Stretch:" << stretch << "Filename:" << p_image;
#endif
  m_reader.setFileName(p_image);
  if (!m_reader.canRead()) {
#ifdef DEBUG_MOVIE
    qDebug() << "[AOLayer::start_playback] Warning! Can't read image:" << p_image;
#endif
      this->kill();
      return;
  }
  last_max_frames = max_frames;
  max_frames = m_reader.imageCount();
  // 0 frames means non-animated image. This changed since qt5 for webp's.
  // manually set it to 1 so nothing breaks.
  if (max_frames == 0)
    max_frames = 1;
  if (m_reader.loopCount() == 0 && max_frames > 1)
    play_once = true;
  if (!continuous
          || ((continuous) && (max_frames != last_max_frames))
          || max_frames == 0
          || frame >= max_frames) {
    frame = 0;
    continuous = false;
  }
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  frame_loader = QtConcurrent::run(thread_pool, this, &AOLayer::populate_vectors);
  #else
  frame_loader = QtConcurrent::run(thread_pool, &AOLayer::populate_vectors, this);
  #endif
#ifdef DEBUG_MOVIE
  qDebug() << "[AOLayer::start_playback] loading frames:" << frame << "/" << max_frames;
#endif
  last_path = p_image;
  while (movie_frames.size() <= frame) // if we haven't loaded the frame we need yet
    frameAdded.wait(&mutex); // wait for the frame loader to add another frame, then check again
  this->set_frame(movie_frames[frame]);

  if (max_frames <= 1) {
    duration = static_duration;
#ifdef DEBUG_MOVIE
    qDebug() << "[AOLayer::start_playback] max_frames is <= 1, using static duration";
#endif
  }
  if (duration > 0 && cull_image == true)
    shfx_timer->start(duration);
#ifdef DEBUG_MOVIE
  qDebug() << "[AOLayer::start_playback] Max frames:" << max_frames << "Setting image to " << p_image
           << "Time taken to process image:" << actual_time.elapsed();

  actual_time.restart();
#endif
}

void CharLayer::play()
{
  if (max_frames <= 1) {
    if (play_once) {
      preanim_timer->start(qMax(0, duration));
    }
    return;
  }
  play_frame_effect(frame);
  AOLayer::play();
}

void AOLayer::play()
{
  if (max_frames <= 1) {
    if (play_once) {
      if (duration > 0)
        ticker->start(duration);
      else
        preanim_done();
    }
    else
      this->freeze();
  }
  else {
    while (movie_delays.size() <= frame) {
        frameAdded.wait(&mutex);
    }
    ticker->start(this->get_frame_delay(movie_delays[frame]));
  }
}

void AOLayer::set_play_once(bool p_play_once) { play_once = p_play_once; }
void AOLayer::set_cull_image(bool p_cull_image) { cull_image = p_cull_image; }
void AOLayer::set_static_duration(int p_static_duration)
{
  static_duration = p_static_duration;
}
void AOLayer::set_max_duration(int p_max_duration)
{
  max_duration = p_max_duration;
}

void CharLayer::load_effects()
{
  movie_effects.clear();
  if (max_frames <= 1)
    return;
  movie_effects.resize(max_frames);
  for (int e_frame = 0; e_frame < max_frames; ++e_frame) {
    QString effect = ao_app->get_screenshake_frame(m_char, m_emote, e_frame);
    if (effect != "") {
      movie_effects[e_frame].append("shake");
    }

    effect = ao_app->get_flash_frame(m_char, m_emote, e_frame);
    if (effect != "") {
      movie_effects[e_frame].append("flash");
    }

    effect = ao_app->get_sfx_frame(m_char, m_emote, e_frame);
    if (effect != "") {
      movie_effects[e_frame].append("sfx^" + effect);
    }
  }
}

void CharLayer::load_network_effects()
{
  movie_effects.clear();
  if (max_frames <= 1)
    return;
  movie_effects.resize(max_frames);
  // Order is important!!!
  QStringList effects_list = {"shake", "flash", "sfx^"};

  // Determines which list is smaller - effects_list or network_strings - and
  // uses it as basis for the loop. This way, incomplete network_strings would
  // still be parsed, and excess/unaccounted for networked information is
  // omitted.
  int effects_size = qMin(effects_list.size(), network_strings.size());

  for (int i = 0; i < effects_size; ++i) {
    QString netstring = network_strings.at(i);
    QStringList emote_splits = netstring.split("^");
    for (const QString &emote : emote_splits) {
      QStringList parsed = emote.split("|");
      if (parsed.size() <= 0 || parsed.at(0) != m_emote)
        continue;
      foreach (QString frame_data, parsed) {
        QStringList frame_split = frame_data.split("=");
        if (frame_split.size() <=
            1) // We might still be hanging at the emote itself (entry 0).
          continue;
        int f_frame = frame_split.at(0).toInt();
        if (f_frame >= max_frames || f_frame < 0) {
          qWarning() << "out of bounds" << effects_list[i] << "frame"
                   << f_frame << "out of" << max_frames << "for" << m_emote;
          continue;
        }
        QString f_data = frame_split.at(1);
        if (f_data != "") {
          QString effect = effects_list[i];
          if (effect == "sfx^") // Currently the only frame result that feeds us
                                // data, let's yank it in.
            effect += f_data;
#ifdef DEBUG_MOVIE
          qDebug() << "[CharLayer::load_network_effects]" << effect << f_data << "frame" << f_frame << "for"
                   << m_emote;
#endif
          movie_effects[f_frame].append(effect);
        }
      }
    }
  }
}

void CharLayer::play_frame_effect(int p_frame)
{
  if (p_frame >= movie_effects.size()) {
    qWarning() << "Attempted to play a frame effect bigger than the size of movie_effects";
    return;
  }
  if (p_frame < max_frames) {
    foreach (QString effect, movie_effects[p_frame]) {
      if (effect == "shake") {
        emit shake();
#ifdef DEBUG_MOVIE
        qDebug() << "[CharLayer::play_frame_effect] Attempting to play shake on frame" << frame;
#endif
      }

      if (effect == "flash") {
        emit flash();
#ifdef DEBUG_MOVIE
        qDebug() << "[CharLayer::play_frame_effect] Attempting to play flash on frame" << frame;
#endif
      }

      if (effect.startsWith("sfx^")) {
        QString sfx = effect.section("^", 1);
        emit play_sfx(sfx);
#ifdef DEBUG_MOVIE
        qDebug() << "[CharLayer::play_frame_effect] Attempting to play sfx" << sfx << "on frame" << frame;
#endif
      }
    }
  }
}

void AOLayer::stop()
{
  // for all intents and purposes, stopping is the same as hiding. at no point
  // do we want a frozen gif to display
  this->freeze();
  this->hide();
  shfx_timer->stop();
}

void AOLayer::freeze()
{
  // aT nO pOiNt Do We WaNt A fRoZeN gIf To DiSpLaY
  ticker->stop();
  preanim_timer->stop();
}

void AOLayer::kill()
{
  // used for when we want to ensure a file is loaded anew
  this->stop();
  this->clear();
  movie_frames.clear();
  movie_delays.clear();
  last_max_frames = max_frames;
  max_frames = 0;
  last_path = "";
}

void CharLayer::movie_ticker()
{
  AOLayer::movie_ticker();
  play_frame_effect(frame);
}

void AOLayer::movie_ticker()
{
  ++frame;
  if (frame >= max_frames) {
    if (play_once) {
      if (cull_image)
        this->stop();
      else
        this->freeze();
      preanim_done();
      return;
    }
    else
      frame = 0;
  }
  {
    QMutexLocker locker(&mutex);
    while (frame >= movie_frames.size() && frame < max_frames) // oops! our frame isn't ready yet
      frameAdded.wait(&mutex); // wait for a new frame to be added, then check again
  }
#ifdef DEBUG_MOVIE
  qDebug() << "[AOLayer::movie_ticker] Frame:" << frame << "Delay:" << movie_delays[frame]
           << "Actual time taken from last frame:" << actual_time.restart()
           << "Animation name:" << m_reader.fileName();
#endif
  this->set_frame(movie_frames[frame]);
  ticker->setInterval(this->get_frame_delay(movie_delays[frame]));
}

void AOLayer::populate_vectors() {
  if (max_frames == 0)
  {
#ifdef DEBUG_MOVIE
    qDebug() << "[AOLayer::populate_vectors] WARNING: 0 frame animation passed. Exiting loop";
#endif
    return;
  }
#ifdef DEBUG_MOVIE
  qDebug() << "[AOLayer::populate_vectors] Started thread";
#endif
  while (!exit_loop && movie_frames.size() < max_frames) {
    load_next_frame();
#ifdef DEBUG_MOVIE
    qDebug() << "[AOLayer::populate_vectors] Loaded frame" << movie_frames.size();
#endif
  }
#ifdef DEBUG_MOVIE
  if (exit_loop)
    qDebug() << "[AOLayer::populate_vectors] Exit requested";
#endif
  exit_loop = false;
}

void AOLayer::load_next_frame() {
  {
    QMutexLocker locker(&mutex);
    movie_frames.append(this->get_pixmap(m_reader.read()));
    movie_delays.append(m_reader.nextImageDelay());
  }
  frameAdded.wakeAll();
}

void CharLayer::preanim_done()
{
  if (is_preanim)
    AOLayer::preanim_done();
  else
    return;
}

void AOLayer::preanim_done()
{
  ticker->stop();
  preanim_timer->stop();
  emit done();
}

void AOLayer::shfx_timer_done()
{
  this->stop();
#ifdef DEBUG_MOVIE
  qDebug() << "shfx timer signaled done";
#endif
  // signal connected to courtroom object, let it figure out what to do
  emit done();
}

void AOLayer::invert() {
  const QPixmap pixmap = this->pixmap();
  QImage *image = new QImage(pixmap.toImage());
  image->invertPixels(QImage::InvertRgb);
  this->setPixmap(QPixmap::fromImage(*image));
}

void AOLayer::onImageLoaded(const QImage& image) {
  disconnect(net_manager, &NetworkManager::imageLoaded, this, &AOLayer::onImageLoaded);
  // qDebug() << "...";
  QPixmap pixmap = get_pixmap(ao_app->net_manager->streamed_image);
  
  if (frame_loader.isRunning())
    exit_loop = true;
  this->show();
  this->clear();
  this->freeze();
  movie_frames.clear();
  movie_delays.clear();
  this->set_frame(pixmap);
  //this->set_frame(get_pixmap(ao_app->net_manager->streamed_image));
  // this->set_frame(ao_app->net_manager->streamed_pixmap);
  // qDebug() << "Started streaming playback.";
}

void AOLayer::fade(bool in, int duration)
{
    QEasingCurve easing = in ? QEasingCurve::OutQuad : QEasingCurve::InQuad;
  
    QGraphicsOpacityEffect* fade = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(fade);
    QPropertyAnimation* fade_anim = new QPropertyAnimation(fade, "opacity");
    fade_anim->setDuration(duration);
    fade_anim->setStartValue(in ? 0 : 1);
    fade_anim->setEndValue(in ? 1 : 0);
    fade_anim->setEasingCurve(easing);
    fade_anim->start(QPropertyAnimation::DeleteWhenStopped);
    if (in) {
      connect(fade_anim, SIGNAL(finished()), this, SLOT(fadein_finished()));
    }
    else {
      connect(fade_anim, SIGNAL(finished()), this, SLOT(fadeout_finished()));
    }
}

void AOLayer::fadeout_finished() {}

void AOLayer::fadein_finished() {}

void BackgroundLayer::fadein_finished() {emit hide_void();}
