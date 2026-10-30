#include "aoapplication.h"

#include "bassmidi.h"
#include "courtroom.h"
#include "debug_functions.h"
#include "file_functions.h"
#include "lobby.h"
#include "networkmanager.h"
#include "options.h"

#include "widgets/aooptionsdialog.h"

#include <QFile>

static QtMessageHandler original_message_handler;
static AOApplication *message_handler_context;
void message_handler(QtMsgType type, const QMessageLogContext &context,
                     const QString &msg)
{
  emit message_handler_context->qt_log_message(type, context, msg);
  original_message_handler(type, context, msg);
}

AOApplication::AOApplication(int &argc, char **argv) : QApplication(argc, argv)
{
  setApplicationName("Attorney Online");
  // On macOS, we distribute with a built-in base in the .app but this is not writeable
  // so we copy the base folder into the preferred base path (~/Library/Application Support/Attorney Online)
  seed_base_if_missing();

  net_manager = new NetworkManager(this);
  discord = new AttorneyOnline::Discord();

  asset_lookup_cache.reserve(2048);

  message_handler_context = this;
  original_message_handler = qInstallMessageHandler(message_handler);

  setApplicationVersion(get_version_string());
  setApplicationDisplayName(tr("Attorney Online Golden: %1").arg(applicationVersion()));

  // Qt5 had hard-coded colors. Qt6 uses Windows accent color for several palette roles,
  // which breaks AO's UI since it wasn't designed with that in mind. Hard-code the colors.

  // Also use cyan link color as the more reasonable default(?)
  qApp->setStyleSheet("QLabel a { color: #00FFFF; }");
  QPalette fixedPalette = qApp->palette();
  fixedPalette.setColor(QPalette::Link, QColor(0, 255, 255));
  fixedPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
  fixedPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  fixedPalette.setColor(QPalette::AlternateBase, QColor(25, 25, 25));
  qApp->setPalette(fixedPalette);
}

AOApplication::~AOApplication()
{
  destruct_lobby();
  destruct_courtroom();
  delete discord;
  qInstallMessageHandler(original_message_handler);
}

void AOApplication::construct_lobby()
{
  if (lobby_constructed) {
    qWarning() << "lobby was attempted constructed when it already exists";
    return;
  }

  w_lobby = new Lobby(this, net_manager);
  lobby_constructed = true;

  QRect geometry = QGuiApplication::primaryScreen()->geometry();
  int x = (geometry.width() - w_lobby->width()) / 2;
  int y = (geometry.height() - w_lobby->height()) / 2;
  w_lobby->move(x, y);

  if (Options::getInstance().discordEnabled())
    discord->state_lobby();

  if (demo_server)
      demo_server->deleteLater();
  demo_server = new DemoServer(this);
  w_lobby->show();
}

void AOApplication::destruct_lobby()
{
  if (!lobby_constructed) {
    qWarning() << "lobby was attempted destructed when it did not exist";
    return;
  }

  delete w_lobby;
  w_lobby = nullptr;
  lobby_constructed = false;
}

void AOApplication::construct_courtroom()
{
  if (courtroom_constructed) {
    qWarning() << "courtroom was attempted constructed when it already exists";
    return;
  }

  w_courtroom = new Courtroom(this);
  courtroom_constructed = true;

  QRect geometry = QGuiApplication::primaryScreen()->geometry();
  int x = (geometry.width() - w_courtroom->width()) / 2;
  int y = (geometry.height() - w_courtroom->height()) / 2;
  w_courtroom->move(x, y);

  if (demo_server != nullptr) {
    QObject::connect(demo_server, &DemoServer::skip_timers,
                     w_courtroom, &Courtroom::skip_clocks);
  }
  else {
    qWarning() << "demo server did not exist during courtroom construction";
  }
}

void AOApplication::destruct_courtroom()
{
  if (!courtroom_constructed) {
    qWarning() << "courtroom was attempted destructed when it did not exist";
    return;
  }

  delete w_courtroom;
  w_courtroom = nullptr;
  courtroom_constructed = false;
}

QString AOApplication::get_version_string()
{
  return GENERATION + " " + QString::number(RELEASE) + "." + QString::number(MAJOR_VERSION) + "." +
         QString::number(MINOR_VERSION) + " (" + VERSION_STATUS + ")";
}

void AOApplication::server_disconnected()
{
  if (m_reconnecting)
      return;

  if (!courtroom_constructed)
      return;

  bool first_attempt = true;

  while (true)
  {
    QMessageBox msgBox;
    msgBox.setText(first_attempt
                    ? tr("Disconnected from server.")
                    : tr("Failed to reconnect to server."));
    QPushButton *btn_lobby =
        msgBox.addButton(tr("Return to Lobby"), QMessageBox::RejectRole);
    QPushButton *btn_reconnect =
        msgBox.addButton(tr("Reconnect"), QMessageBox::AcceptRole);
    msgBox.setDefaultButton(btn_lobby);
    msgBox.exec();
    m_reconnecting = false;
    if (msgBox.clickedButton() == btn_lobby)
    {
      Options::getInstance().setServerSubTheme(QString());
      construct_lobby();
      destruct_courtroom();
      return;
    }
    // Reconnect has been chosen! Make sure we're marked as reconnecting.
    first_attempt = false;
    m_reconnecting = true;
    net_manager->connect_to_server(net_manager->last_server_chosen);

    // Make sure to do an event loop just in case we quit out early
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    bool success = false;
    const QMetaObject::Connection conn =
        connect(net_manager, &NetworkManager::server_connected, this,
                [&](bool state) {
                    success = state;
                    loop.quit();
                });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    // Timeout in 10 seconds
    timeout.start(10'000);

    loop.exec();

    disconnect(conn);
    m_reconnecting = false;

    if (success)
    {
      net_manager->join_to_server();
      const QString hdid = get_hdid();
      send_server_packet(new AOPacket("RD"));
      send_server_packet(new AOPacket("CC", {QString::number(client_id),
                                              QString::number(w_courtroom->get_cid()),
                                              hdid}));
      call_notice(tr("Success reconnecting to server."), 0);
      return;
    }
    // If failed or timed out, re-show the same notice
  }
}

void AOApplication::loading_cancelled()
{
  destruct_courtroom();
}

void AOApplication::call_settings_menu(int initial_tab)
{
    try {
      AOOptionsDialog* l_dialog = new AOOptionsDialog(nullptr, this, initial_tab);
      if (courtroom_constructed) {
          connect(l_dialog, &AOOptionsDialog::reloadThemeRequest,
                  w_courtroom, &Courtroom::on_reload_theme_clicked);
      }

      if(lobby_constructed) {
      }
      l_dialog->exec();
      if (courtroom_constructed) {
        w_courtroom->playerList()->reloadPlayers();
      }
      delete l_dialog;

    } catch (const std::exception &e) {
      qCritical() << "Exception caught:" << e.what();

      QFile logFile("error.log");
      if (logFile.open(QIODevice::Append | QIODevice::Text)) {
          QTextStream stream(&logFile);
          stream << "Exception caught: " << e.what() << "\n";
          logFile.close();
      } else {
          qCritical() << "Failed to open log file.";
    }
  }
}

// Callback for when BASS device is lost
// Only actually used for music syncs
void CALLBACK AOApplication::BASSreset(HSTREAM handle, DWORD channel,
                                       DWORD data, void *user)
{
  Q_UNUSED(handle);
  Q_UNUSED(channel);
  Q_UNUSED(data);
  Q_UNUSED(user);
  doBASSreset();
}

void AOApplication::doBASSreset()
{
  BASS_Free();
  BASS_Init(-1, 48000, BASS_DEVICE_LATENCY, nullptr, nullptr);
  load_bass_plugins();
}

void AOApplication::initBASS()
{
  BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, 1);
  BASS_Free();
  // Change the default audio output device to be the one the user has given
  // in his config.ini file for now.
  unsigned int a = 0;
  BASS_DEVICEINFO info;

  if (Options::getInstance().audioOutputDevice() == "default") {
    BASS_Init(-1, 48000, BASS_DEVICE_LATENCY, nullptr, nullptr);
    load_bass_plugins();
  }
  else {
    for (a = 0; BASS_GetDeviceInfo(a, &info); a++) {
      if (Options::getInstance().audioOutputDevice() == info.name) {
        BASS_SetDevice(a);
        BASS_Init(static_cast<int>(a), 48000, BASS_DEVICE_LATENCY, nullptr,
                  nullptr);
        load_bass_plugins();
        qInfo() << info.name << "was set as the default audio output device.";
        BASS_SetConfigPtr(BASS_CONFIG_MIDI_DEFFONT, QString(get_base_path() + "soundfont.sf2").toStdString().c_str());
        return;
      }
    }
    BASS_Init(-1, 48000, BASS_DEVICE_LATENCY, nullptr, nullptr);
    load_bass_plugins();
  }
  BASS_SetConfigPtr(BASS_CONFIG_MIDI_DEFFONT, QString(get_base_path() + "soundfont.sf2").toStdString().c_str());
}

#if (defined(_WIN32) || defined(_WIN64))
void AOApplication::load_bass_plugins()
{
  BASS_PluginLoad("bassopus.dll", 0);
  BASS_PluginLoad("bassmidi.dll", 0);
}
#elif defined __APPLE__
void AOApplication::load_bass_plugins()
{
  BASS_PluginLoad("libbassopus.dylib", 0);
  BASS_PluginLoad("libbassmidi.dylib", 0);
}
#elif (defined(LINUX) || defined(__linux__))
void AOApplication::load_bass_plugins()
{
  BASS_PluginLoad("libbassopus.so", 0);
  BASS_PluginLoad("libbassmidi.so", 0);
}
#else
#error This operating system is unsupported for BASS plugins.
#endif
