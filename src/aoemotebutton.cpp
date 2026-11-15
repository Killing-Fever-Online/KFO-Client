#include "aoemotebutton.h"
#include "file_functions.h"
#include "webcache.h"

AOEmoteButton::AOEmoteButton(QWidget *p_parent, AOApplication *p_ao_app,
                             int p_x, int p_y, int p_w, int p_h)
    : QPushButton(p_parent)
{
  parent = p_parent;
  ao_app = p_ao_app;

  this->move(p_x, p_y);
  this->resize(p_w, p_h);

  ui_selected = new QLabel(this);
  ui_selected->resize(size());
  ui_selected->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_selected->hide();

  connect(this, &AOEmoteButton::clicked, this, &AOEmoteButton::on_clicked);
  connect(ao_app->webcache(), &WebCache::fileDownloaded, this, &AOEmoteButton::set_webpath);
}

void AOEmoteButton::set_selected_image(QString p_image)
{
  if (file_exists(p_image)) {
    ui_selected->setStyleSheet("border-image: url(\"" + p_image + "\")");
  }
  else {
    ui_selected->setStyleSheet("background-color: rgba(0, 0, 0, 128)");
  }
}

void AOEmoteButton::set_image(QString p_image, QString p_emote_comment)
{
  if (file_exists(p_image)) {
    this->setText("");
    this->setStyleSheet(
        "QPushButton { border: none; }"
        "QToolTip { color: #000000; background-color: #ffffff; border: 0px; }");
    this->setIcon(QPixmap(p_image).scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    this->setIconSize(this->size());
  }
  else {
    this->setText(p_emote_comment);
    this->setStyleSheet("QPushButton { border-image: url(); }"
                        "QToolTip { background-image: url(); color: #000000; "
                        "background-color: #ffffff; border: 0px; }");
    this->setIcon(QIcon());
    this->setIconSize(this->size());
  }
}

void AOEmoteButton::set_char_image(QString p_char, int p_emote, bool on)
{
  QString emotion_number = QString::number(p_emote + 1);
  QStringList suffixes { "_off", "_on" };
  QStringList suffixedPaths;
  for (const QString &suffix : suffixes) {
    suffixedPaths.append(ao_app->get_image_suffix(ao_app->get_character_path(
        p_char, "emotions/button" + emotion_number + suffix)));
  }
  QString image = suffixedPaths[static_cast<int>(on)];

  emote_comment = ao_app->get_emote_comment(p_char, p_emote);
  if (on && !file_exists(suffixedPaths[1])) {;
    ui_selected->show();
    image = suffixedPaths[0];
  }
  else {
    ui_selected->hide();
  }
  // WebCache DL the emote button (for simplicity's sake it only DLs the _off button
  if (Options::getInstance().webcacheEnabled() && !file_exists(image)) {
    QString image_path = ao_app->get_character_path(p_char, "emotions/button" + emotion_number + suffixes[0]).toQString();
    const QStringList extensions = ao_app->assetExtensions("emotions_extensions", {".png", ".webp"});
    if (!extensions.isEmpty()) {
      QString lowerPath = ao_app->webcache()->resolve(image_path, extensions);
      qDebug() << "WebCache: resolving emote button of " << image_path;
      if (!lowerPath.isEmpty()) {
        qDebug() << "WebCache: starting download of emote button" << lowerPath;
        download_path = lowerPath;
        ao_app->webcache()->download(lowerPath);
        return;
      }
    }
  }
  set_image(image, emote_comment);
}

void AOEmoteButton::set_webpath(const QString &relativePath)
{
  if (relativePath != download_path)
  {
    return;
  }
  QString real_path = ao_app->get_real_path(VPath(relativePath));
  qDebug() << "Success, setting emote to path " << real_path << " with comment " << emote_comment;
  this->set_image(real_path, emote_comment);
}

void AOEmoteButton::on_clicked() { emit emote_clicked(m_id); }

