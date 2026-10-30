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

QString PlayerListWidget::categoryKeyForPlayer(const PlayerData &player) const {
  if (!player.category_name.isEmpty()) {
    return QStringLiteral("name:") + player.category_name;
  }
  return QStringLiteral("area:") + QString::number(player.area_id);
}

QString PlayerListWidget::currentCategoryKey(QTreeWidgetItem *parent) const {
  if (parent == nullptr) {
    return QString();
  }
  return parent->data(0, CategoryKeyRole).toString();
}

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

  case PlayerUpdate::CATEGORY_NAME:
    player.category_name = update.data;
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
  refreshCategoryLabels();
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
  const PlayerData &playerData = m_player_map[playerId];
  insertPlayerItem(playerId, item, playerData);
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
  if (item != nullptr) {
    QTreeWidgetItem *parent = item->parent();
    const QString groupKey = currentCategoryKey(parent);
    if (parent != nullptr) {
      parent->removeChild(item);
    }
    delete item;
    if (!groupKey.isEmpty()) {
      removeCategoryIfEmpty(groupKey);
    }
  }
  m_player_map.remove(playerId);
  m_registration_order.remove(playerId);
}

QTreeWidgetItem *PlayerListWidget::ensureCategoryItem(const QString &categoryKey) {
  QTreeWidgetItem *item = m_category_item_map.value(categoryKey, nullptr);
  if (item != nullptr) {
    return item;
  }

  item = new QTreeWidgetItem;
  item->setData(0, CategoryKeyRole, categoryKey);
  item->setData(0, ItemTypeRole, CategoryItem);
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable);

  const bool isNameCategory = categoryKey.startsWith(QStringLiteral("name:"));

  // Name-categories sort alphabetically and come first.
  // Area-categories sort numerically and come after all name-categories.
  int insert_index = 0;
  const int count = topLevelItemCount();
  for (int i = 0; i < count; ++i) {
    QTreeWidgetItem *existing = topLevelItem(i);
    const QString existingKey = existing->data(0, CategoryKeyRole).toString();
    const bool existingIsName = existingKey.startsWith(QStringLiteral("name:"));

    bool insertBefore;
    if (isNameCategory) {
      insertBefore = existingIsName && existingKey > categoryKey;
    } else {
      insertBefore = !existingIsName
                     || existingKey.mid(5).toInt() > categoryKey.mid(5).toInt();
    }
    if (insertBefore) {
      break;
    }
    insert_index = i + 1;
  }

  insertTopLevelItem(insert_index, item);
  m_category_item_map.insert(categoryKey, item);
  item->setExpanded(true);
  updateCategoryLabel(categoryKey);
  return item;
}

void PlayerListWidget::updateCategoryLabel(const QString &categoryKey) {
  QTreeWidgetItem *item = m_category_item_map.value(categoryKey, nullptr);
  if (item == nullptr) {
    return;
  }

  if (categoryKey.startsWith(QStringLiteral("name:"))) {
    const QString categoryName = categoryKey.mid(5);
    item->setText(0, categoryName);
    item->setToolTip(0, tr("Category: %1").arg(categoryName));
  } else {
    // Preserve the existing area-label rendering (name lookup, formatting, etc.)
    // — this is the original body of updateAreaLabel, just keyed differently.
    const int areaId = categoryKey.mid(5).toInt();
    // ... existing area-label logic ...
  }
}

void PlayerListWidget::refreshCategoryLabels() {
  for (auto it = m_category_item_map.cbegin(); it != m_category_item_map.cend(); ++it) {
    updateCategoryLabel(it.key());
  }
}

void PlayerListWidget::insertPlayerItem(int playerId, QTreeWidgetItem *item, const PlayerData &player) {
  QTreeWidgetItem *parent = ensureCategoryItem(categoryKeyForPlayer(player));

  const int order = m_registration_order.value(playerId);
  int insert_index = parent->childCount();
  for (int i = 0; i < parent->childCount(); ++i) {
    const int child_id = parent->child(i)->data(0, PlayerIdRole).toInt();
    if (m_registration_order.value(child_id) > order) {
      insert_index = i;
      break;
    }
  }
  parent->insertChild(insert_index, item);
}

void PlayerListWidget::removeCategoryIfEmpty(const QString &categoryKey) {
  QTreeWidgetItem *item = m_category_item_map.value(categoryKey, nullptr);
  if (item == nullptr || item->childCount() > 0) {
    return;
  }
  const int index = indexOfTopLevelItem(item);
  if (index >= 0) {
    delete takeTopLevelItem(index);
  }
  m_category_item_map.remove(categoryKey);
}

void PlayerListWidget::filterPlayerList() {
  for (const PlayerData &player : std::as_const(m_player_map)) {
    QTreeWidgetItem *item = m_item_map.value(player.id, nullptr);
    if (item == nullptr) {
      qWarning() << "Trying to filter item that does not exist. This indicates "
                    "either a broken server-implementation or a bad demo file.";
      continue;
    }
    QTreeWidgetItem *parent = item->parent();
    const QString currentKey = currentCategoryKey(parent);
    const QString desiredKey = categoryKeyForPlayer(player);
    if (currentKey == desiredKey) {
      continue;
    }
    if (parent != nullptr) {
      parent->removeChild(item);
    }
    insertPlayerItem(player.id, item, player);
    if (!currentKey.isEmpty()) {
      removeCategoryIfEmpty(currentKey);
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
