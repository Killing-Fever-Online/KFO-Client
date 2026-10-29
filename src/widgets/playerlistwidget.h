#ifndef PLAYERLISTWIDGET_H
#define PLAYERLISTWIDGET_H

#include "datatypes.h"

#include <QMap>
#include <QPair>
#include <QPointer>
#include <QStringList>
#include <QTreeWidget>

class AOApplication;
class ModeratorDialog;

// In-courtroom list of the players currently connected, grouped into
// expandable areas. Populated by the AO 2.11 "PR" (register) and "PU" (update)
// packets.
class PlayerListWidget : public QTreeWidget
{
  Q_OBJECT

public:
  explicit PlayerListWidget(AOApplication *ao_app, QWidget *parent = nullptr);
  ~PlayerListWidget() override;

  void registerPlayer(const PlayerRegister &update);
  void updatePlayer(const PlayerUpdate &update);
  void reloadPlayers();

  // Sets the area names used as group headers. The index in the list matches
  // PlayerData::area_id.
  void setAreaNames(const QStringList &names);

  void setAuthenticated(bool f_state);
  // Highlights the player currently selected for pairing (-1 clears it).
  void setPairedPlayer(int playerId);

  QString playerlistFormatString() const;
  void setPlayerlistFormatString(QString value);

private:
  enum ItemDataRole {
    PlayerIdRole = Qt::UserRole,
    AreaIdRole,
    ItemTypeRole,
  };

  enum ItemType {
    PlayerItem,
    AreaItem,
  };

  AOApplication *ao_app;
  QMap<int, PlayerData> m_player_map;
  QMap<int, QTreeWidgetItem *> m_item_map;
  QMap<int, QTreeWidgetItem *> m_area_item_map;
  QMap<int, int> m_registration_order;
  int m_next_registration_order = 0;
  QStringList m_area_names;
  QPair<int, QPointer<ModeratorDialog>> active_moderator_menu;
  bool m_is_authenticated = false;
  int m_paired_player_id = -1;

  QString m_format_string = "";

  void addPlayer(int playerId);
  void removePlayer(int playerId);
  void updatePlayer(int playerId, bool updateIcon);

  QTreeWidgetItem *ensureAreaItem(int areaId);
  void updateAreaLabel(int areaId);
  void refreshAreaLabels();
  void insertPlayerItem(int playerId, QTreeWidgetItem *item, int areaId);
  void removeAreaIfEmpty(int areaId);

  QString formatLabel(const PlayerData &data);

  void filterPlayerList();

Q_SIGNALS:
  void notify(const QString &message);
  // Emitted from the context menu to pair with a player. A negative playerId
  // clears the current pairing.
  void pairRequested(int playerId, const QString &character);

private Q_SLOTS:
  void onCustomContextMenuRequested(const QPoint &pos);
};

#endif // PLAYERLISTWIDGET_H
