#ifndef AOBLIPPLAYER_H
#define AOBLIPPLAYER_H

#include "bass.h"
#include "bassopus.h"

#include "aoapplication.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QWidget>
#include <string.h>

class AOBlipPlayer : public QObject {
  Q_OBJECT
public:
  AOBlipPlayer(QWidget *parent, AOApplication *p_ao_app);

  void set_blips(QString p_sfx);
  void blip_tick();
  void set_volume(int p_volume);
  void set_muted(bool toggle);

  int m_cycle = 0;

private:
  QWidget *m_parent;
  AOApplication *ao_app;
  qreal m_volume;

  bool m_muted = false;

  void set_volume_internal(qreal p_volume);

  HSTREAM m_stream_list[5];

  QString m_pending_sfx;
  QString m_pending_path;

private slots:
  void on_webcache_file_downloaded(const QString &relativePath);
  void on_webcache_download_failed(const QString &relativePath);
};

#endif // AOBLIPPLAYER_H
