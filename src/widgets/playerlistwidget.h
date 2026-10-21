#ifndef PLAYERLISTWIDGET_H
#define PLAYERLISTWIDGET_H

#include "datatypes.h"

#include <QListWidget>
#include <QMap>
#include <QPair>
#include <QPointer>

class AOApplication;
class ModeratorDialog;

// In-courtroom list of the players currently connected to the area. Populated
// by the AO 2.11 "PR" (register) and "PU" (update) packets.
class PlayerListWidget : public QListWidget
{
  Q_OBJECT

public:
  explicit PlayerListWidget(AOApplication *ao_app, QWidget *parent = nullptr);
  ~PlayerListWidget() override;

  void registerPlayer(const PlayerRegister &update);
  void updatePlayer(const PlayerUpdate &update);
  void reloadPlayers();

  void setAuthenticated(bool f_state);
  // Highlights the player currently selected for pairing (-1 clears it).
  void setPairedPlayer(int playerId);

private:
  AOApplication *ao_app;
  QMap<int, PlayerData> m_player_map;
  QMap<int, QListWidgetItem *> m_item_map;
  QPair<int, QPointer<ModeratorDialog>> active_moderator_menu;
  bool m_is_authenticated = false;
  int m_paired_player_id = -1;

  void addPlayer(int playerId);
  void removePlayer(int playerId);
  void updatePlayer(int playerId, bool updateIcon);

  QString formatLabel(const PlayerData &data);

  void filterPlayerList();

Q_SIGNALS:
  void notify(const QString &message);
  // Emitted when a player is left-clicked. The character is empty when the
  // player has not yet picked one (e.g. a spectator).
  void playerClicked(int playerId, const QString &character);

private Q_SLOTS:
  void onCustomContextMenuRequested(const QPoint &pos);
  void onItemClicked(QListWidgetItem *item);
};

#endif // PLAYERLISTWIDGET_H
