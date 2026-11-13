#include "aoblipplayer.h"
#include "file_functions.h"
#include "options.h"
#include "webcache.h"

AOBlipPlayer::AOBlipPlayer(QWidget *parent, AOApplication *p_ao_app)
    : QObject(p_ao_app)
{
  m_parent = parent;
  ao_app = p_ao_app;

  connect(ao_app->webcache(), &WebCache::fileDownloaded, this, &AOBlipPlayer::on_webcache_file_downloaded);
  connect(ao_app->webcache(), &WebCache::downloadFailed, this, &AOBlipPlayer::on_webcache_download_failed);
}

void AOBlipPlayer::set_blips(QString p_sfx)
{
  QString f_path = ao_app->get_sfx_suffix(ao_app->get_sounds_path(p_sfx));

  if (!file_exists(f_path) && Options::getInstance().webcacheEnabled())
  {
    const QStringList extensions = ao_app->assetExtensions(
        "blip_extensions", {".opus", ".ogg", ".mp3", ".wav"});

    if (!extensions.isEmpty())
    {
      const QString lowerPath = ao_app->webcache()->resolve(ao_app->get_sounds_path(p_sfx).toQString(), extensions);
      if (!lowerPath.isEmpty())
      {
        m_pending_sfx = p_sfx;
        m_pending_path = lowerPath;
        ao_app->webcache()->download(lowerPath);
        return;
      }
    }
  }

  for (int n_stream = 0; n_stream < 5; ++n_stream) {
    BASS_StreamFree(m_stream_list[n_stream]);

    if (f_path.endsWith(".opus"))
      m_stream_list[n_stream] = BASS_OPUS_StreamCreateFile(
          FALSE, f_path.utf16(), 0, 0, BASS_UNICODE | BASS_ASYNCFILE);
    else
      m_stream_list[n_stream] = BASS_StreamCreateFile(
          FALSE, f_path.utf16(), 0, 0, BASS_UNICODE | BASS_ASYNCFILE);
  }

  set_volume_internal(m_volume);
}

void AOBlipPlayer::blip_tick()
{
  int f_cycle = m_cycle++;

  if (m_cycle == 5)
    m_cycle = 0;

  HSTREAM f_stream = m_stream_list[f_cycle];

  BASS_ChannelSetDevice(f_stream, BASS_GetDevice());
  BASS_ChannelPlay(f_stream, false);
}

void AOBlipPlayer::set_muted(bool toggle)
{
  m_muted = toggle;
  set_volume_internal(m_volume);
}

void AOBlipPlayer::set_volume(int p_value)
{
  m_volume = static_cast<qreal>(p_value) / 100;
  set_volume_internal(m_volume);
}

void AOBlipPlayer::set_volume_internal(qreal p_value)
{
  // If muted, volume will always be 0
  float volume = static_cast<float>(p_value) * !m_muted;

  for (int n_stream = 0; n_stream < 5; ++n_stream) {
    BASS_ChannelSetAttribute(m_stream_list[n_stream], BASS_ATTRIB_VOL, volume);
  }
}

void AOBlipPlayer::on_webcache_file_downloaded(const QString &relativePath)
{
  if (m_pending_path.isEmpty() || relativePath != m_pending_path)
  {
    return;
  }

  const QString pending_sfx = m_pending_sfx;
  m_pending_sfx.clear();
  m_pending_path.clear();
  set_blips(pending_sfx);
}

void AOBlipPlayer::on_webcache_download_failed(const QString &relativePath)
{
  if (m_pending_path.isEmpty() || relativePath != m_pending_path)
  {
    return;
  }

  const QString pending_sfx = m_pending_sfx;
  m_pending_sfx.clear();
  m_pending_path.clear();
  // Re-run resolution so the next advertised extension can be attempted.
  set_blips(pending_sfx);
}
