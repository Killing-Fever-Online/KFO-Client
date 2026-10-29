#include "playerlistwidget.h"

#include "aoapplication.h"
#include "aopacket.h"
#include "courtroom.h"
#include "moderation_functions.h"
#include "options.h"
#include "widgets/moderator_dialog.h"

#include <QAction>
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QTreeWidgetItem>

#include <utility>

PlayerListWidget::PlayerListWidget(AOApplication *ao_app, QWidget *parent)
    : QTreeWidget(parent)
    , ao_app(ao_app)
{
  setHeaderHidden(true);
  setRootIsDecorated(true);
  setUniformRowHeights(true);
  setIndentation(6);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setContextMenuPolicy(Qt::CustomContextMenu);
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  setStyleSheet("QTreeWidget { background-color: gray; }");

  connect(this, &PlayerListWidget::customContextMenuRequested, this,
          &PlayerListWidget::onCustomContextMenuRequested);
}

PlayerListWidget::~PlayerListWidget() {}

void PlayerListWidget::registerPlayer(const PlayerRegister &update)
{
  switch (update.type)
  {
  default:
    Q_UNREACHABLE();
    break;

  case PlayerRegister::ADD_PLAYER:
    addPlayer(update.id);
    break;

  case PlayerRegister::REMOVE_PLAYER:
    removePlayer(update.id);
    break;
  }
}

void PlayerListWidget::updatePlayer(const PlayerUpdate &update)
{
  PlayerData &player = m_player_map[update.id];

  bool update_icon = false;
  switch (update.type)
  {
  default:
    Q_UNREACHABLE();
    break;

  case PlayerUpdate::NAME:
    player.name = update.data;
    break;

  case PlayerUpdate::CHARACTER:
    player.character = update.data;
    update_icon = true;
    break;

  case PlayerUpdate::CHARACTER_NAME:
    player.character_name = update.data;
    break;

  case PlayerUpdate::AREA_ID:
    player.area_id = update.data.toInt();
    break;

  case PlayerUpdate::DESC:
    player.desc = update.data;
    break;

  case PlayerUpdate::FORMAT:
    player.format = update.data;
    break;

  case PlayerUpdate::COMMANDS:
    player.commands = update.data.split("&");
    break;
  }
  updatePlayer(player.id, update_icon);

  filterPlayerList();
}

void PlayerListWidget::reloadPlayers()
{
  for (const PlayerData &player : std::as_const(m_player_map))
  {
    updatePlayer(player.id, false);
  }
}

void PlayerListWidget::setAreaNames(const QStringList &names)
{
  if (m_area_names == names)
  {
    return;
  }

  m_area_names = names;
  refreshAreaLabels();
}

void PlayerListWidget::setPairedPlayer(int playerId)
{
  if (m_paired_player_id == playerId)
  {
    return;
  }

  m_paired_player_id = playerId;
  for (const PlayerData &player : std::as_const(m_player_map))
  {
    updatePlayer(player.id, false);
  }
}


void PlayerListWidget::setAuthenticated(bool f_state)
{
  m_is_authenticated = f_state;
  for (const PlayerData &data : std::as_const(m_player_map))
  {
    updatePlayer(data.id, false);
    filterPlayerList();
  }
}

void PlayerListWidget::onCustomContextMenuRequested(const QPoint &pos)
{
  QTreeWidgetItem *item = itemAt(pos);
  if (item == nullptr ||
      item->data(0, ItemTypeRole).toInt() != PlayerItem)
  {
    return;
  }
  int id = item->data(0, PlayerIdRole).toInt();
  QString name = item->text(0);

  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  PlayerData player_data = m_player_map.value(id);

  if (id == m_paired_player_id)
  {
    QAction *unpair_action = menu->addAction(tr("Unpair"));
    connect(unpair_action, &QAction::triggered, this,
            [this] { Q_EMIT pairRequested(-1, QString()); });
  }
  else if (!player_data.character.isEmpty())
  {
    const QString character = player_data.character;
    QAction *pair_action = menu->addAction(tr("Pair with %1").arg(name));
    connect(pair_action, &QAction::triggered, this, [this, id, character] {
      Q_EMIT pairRequested(id, character);
    });
  }

  // Special server-defined commands
  if (!player_data.commands.isEmpty())
  {
    // Separator for the commands
    menu->addSeparator();
    for (const QString &command : player_data.commands) {
      QStringList split = command.split("{}");
      QString send_command = split[0];
      QString action_name = send_command;

      // Separator provided so we also get a command name
      if (split.size() > 0)
      {
        action_name = split[0];
        send_command = split[1];
      }

      send_command = send_command
          .replace("{id}", QString::number(player_data.id))
          .replace("{character}", player_data.character)
          .replace("{displayname}", player_data.character_name)
          .replace("{username}", player_data.name)
          .replace("{desc}", player_data.desc);
      QAction *custom_command_action = menu->addAction(action_name);
      connect(custom_command_action, &QAction::triggered, this, [this, send_command] {
        ao_app->send_server_packet(
            new AOPacket("CT", {ao_app->w_courtroom->ooc_name, send_command}));
      });
    }
  }

  // Separator for the administrative section
  menu->addSeparator();

  QAction *report_player_action = menu->addAction("Report Player");
  connect(report_player_action, &QAction::triggered, this, [this, id, name] {
    auto maybe_reason = call_moderator_support(name);
    if (maybe_reason.has_value())
    {
      ao_app->send_server_packet(
          new AOPacket("ZZ", {maybe_reason.value(), QString::number(id)}));
    }
  });

  if (m_is_authenticated)
  {
    QAction *kick_player_action = menu->addAction("Kick");
    connect(kick_player_action, &QAction::triggered, this, [this, id, name] {
      ModeratorDialog *dialog = new ModeratorDialog(id, false, ao_app);
      dialog->setWindowTitle(tr("Kick %1").arg(name));
      connect(this, &PlayerListWidget::destroyed, dialog,
              &ModeratorDialog::deleteLater);
      active_moderator_menu = {id, dialog};
      dialog->show();
    });

    QAction *ban_player_action = menu->addAction("Ban");
    connect(ban_player_action, &QAction::triggered, this, [this, id, name] {
      ModeratorDialog *dialog = new ModeratorDialog(id, true, ao_app);
      dialog->setWindowTitle(tr("Ban %1").arg(name));
      connect(this, &PlayerListWidget::destroyed, dialog,
              &ModeratorDialog::deleteLater);
      active_moderator_menu = {id, dialog};
      dialog->show();
    });
  }

  menu->popup(mapToGlobal(pos));
}

void PlayerListWidget::addPlayer(int playerId)
{
  m_player_map.insert(playerId, PlayerData{.id = playerId});
  m_registration_order.insert(playerId, m_next_registration_order++);

  QTreeWidgetItem *item = new QTreeWidgetItem;
  item->setData(0, PlayerIdRole, playerId);
  item->setData(0, ItemTypeRole, PlayerItem);
  insertPlayerItem(playerId, item, 0);
  m_item_map.insert(playerId, item);
  updatePlayer(playerId, false);
}

void PlayerListWidget::removePlayer(int playerId)
{
  if (active_moderator_menu.first == playerId && active_moderator_menu.second)
  {
    delete active_moderator_menu.second;
    Q_EMIT notify("Closed Moderation Dialog : User left the server.");
  }

  if (m_paired_player_id == playerId)
  {
    m_paired_player_id = -1;
  }

  QTreeWidgetItem *item = m_item_map.take(playerId);
  if (item != nullptr)
  {
    QTreeWidgetItem *parent = item->parent();
    const int area_id =
        parent != nullptr ? parent->data(0, AreaIdRole).toInt() : -1;
    if (parent != nullptr)
    {
      parent->removeChild(item);
    }
    delete item;
    if (area_id >= 0)
    {
      removeAreaIfEmpty(area_id);
    }
  }

  m_player_map.remove(playerId);
  m_registration_order.remove(playerId);
}

QTreeWidgetItem *PlayerListWidget::ensureAreaItem(int areaId)
{
  QTreeWidgetItem *area_item = m_area_item_map.value(areaId, nullptr);
  if (area_item != nullptr)
  {
    return area_item;
  }

  area_item = new QTreeWidgetItem;
  area_item->setData(0, AreaIdRole, areaId);
  area_item->setData(0, ItemTypeRole, AreaItem);
  area_item->setFlags(area_item->flags() & ~Qt::ItemIsSelectable);

  // Keep the top-level groups ordered numerically by area id. Sorting by text
  // would put "Area 10" before "Area 2".
  int insert_index = 0;
  while (insert_index < topLevelItemCount() &&
         topLevelItem(insert_index)->data(0, AreaIdRole).toInt() < areaId)
  {
    ++insert_index;
  }
  insertTopLevelItem(insert_index, area_item);
  m_area_item_map.insert(areaId, area_item);
  area_item->setExpanded(true);
  updateAreaLabel(areaId);
  return area_item;
}

void PlayerListWidget::updateAreaLabel(int areaId)
{
  QTreeWidgetItem *area_item = m_area_item_map.value(areaId, nullptr);
  if (area_item == nullptr)
  {
    return;
  }

  QString name = m_area_names.value(areaId);
  if (name.isEmpty())
  {
    name = tr("Area %1").arg(areaId);
  }
  area_item->setText(0, name);
  area_item->setToolTip(0, tr("Area ID: %1").arg(areaId));
}

void PlayerListWidget::refreshAreaLabels()
{
  for (auto it = m_area_item_map.cbegin(); it != m_area_item_map.cend(); ++it)
  {
    updateAreaLabel(it.key());
  }
}

void PlayerListWidget::insertPlayerItem(int playerId, QTreeWidgetItem *item,
                                        int areaId)
{
  QTreeWidgetItem *area_item = ensureAreaItem(areaId);

  // Keep players in registration order inside their area group.
  const int order = m_registration_order.value(playerId);
  int insert_index = area_item->childCount();
  for (int i = 0; i < area_item->childCount(); ++i)
  {
    const int child_id = area_item->child(i)->data(0, PlayerIdRole).toInt();
    if (m_registration_order.value(child_id) > order)
    {
      insert_index = i;
      break;
    }
  }
  area_item->insertChild(insert_index, item);
}

void PlayerListWidget::removeAreaIfEmpty(int areaId)
{
  QTreeWidgetItem *area_item = m_area_item_map.value(areaId, nullptr);
  if (area_item == nullptr || area_item->childCount() > 0)
  {
    return;
  }

  const int index = indexOfTopLevelItem(area_item);
  if (index >= 0)
  {
    delete takeTopLevelItem(index);
  }
  m_area_item_map.remove(areaId);
}

void PlayerListWidget::filterPlayerList()
{
  for (const PlayerData &player : std::as_const(m_player_map))
  {
    QTreeWidgetItem *item = m_item_map.value(player.id, nullptr);
    if (item == nullptr)
    {
      qWarning() << "Trying to filter item that does not exist. This indicates "
                    "either a broken server-implementation or a bad demo file.";
      continue;
    }

    QTreeWidgetItem *parent = item->parent();
    const int current_area =
        parent != nullptr ? parent->data(0, AreaIdRole).toInt() : -1;
    if (current_area == player.area_id)
    {
      continue;
    }

    if (parent != nullptr)
    {
      parent->removeChild(item);
    }
    insertPlayerItem(player.id, item, player.area_id);
    if (current_area >= 0)
    {
      removeAreaIfEmpty(current_area);
    }
  }
}

void PlayerListWidget::updatePlayer(int playerId, bool updateIcon)
{
  PlayerData &data = m_player_map[playerId];
  QTreeWidgetItem *item = m_item_map.value(playerId, nullptr);

  if (!item)
  {
    qWarning() << "No player at ID" << playerId
               << ". This might indicate a broker server implementation or a "
                  "bad demo file.";
    return;
  }

  item->setText(0, formatLabel(data));

  QFont font = item->font(0);
  font.setBold(playerId == m_paired_player_id);
  item->setFont(0, font);

  if (updateIcon)
  {
    QString icon_path;
    if (!data.character.isEmpty())
    {
      icon_path = ao_app->get_image_suffix(
          ao_app->get_character_path(data.character, "char_icon"), true);
    }
    item->setIcon(0, icon_path.isEmpty() ? QIcon() : QIcon(icon_path));
  }

  if (data.character.isEmpty())
  {
    item->setToolTip(0, QString());
    return;
  }

  QString tooltip = data.character;
  if (!data.character_name.isEmpty())
  {
    tooltip = data.character_name;
  }
  if (!data.desc.isEmpty())
  {
    tooltip += "\n" + data.desc;
  }
  item->setToolTip(0, tooltip);
}

QString PlayerListWidget::playerlistFormatString() const
{
  if (m_format_string.isEmpty())
    return Options::getInstance().playerlistFormatString();
  return m_format_string;
}

QString PlayerListWidget::formatLabel(const PlayerData &data)
{
  QString format = Options::getInstance().playerlistFormatString();

  // Special formatting defined just for this entry
  if (!data.format.isEmpty()) {
    format = data.format;
  }
  // Make sure to replace the items as needed
  return format
      .replace("{id}", QString::number(data.id))
      .replace("{character}", data.character)
      .replace("{displayname}", data.character_name)
      .replace("{username}", data.name)
      .replace("{desc}", data.desc)
      .simplified();
}
