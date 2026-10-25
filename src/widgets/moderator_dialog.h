#ifndef MODERATOR_DIALOG_H
#define MODERATOR_DIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSpinBox>
#include <QTextEdit>
#include <QWidget>

class AOApplication;

// Modal used from the player list context menu to kick or ban a player. The
// confirmation is sent to the server as a "MA" packet.
class ModeratorDialog : public QWidget
{
  Q_OBJECT

public:
  static const QString UI_FILE_PATH;

  explicit ModeratorDialog(int clientId, bool ban, AOApplication *ao_app,
                           QWidget *parent = nullptr);
  ~ModeratorDialog() override;

private:
  AOApplication *ao_app;
  int m_client_id;
  bool m_ban;

  QWidget *ui_widget;
  QComboBox *ui_action;
  QSpinBox *ui_duration_mm;
  QSpinBox *ui_duration_hh;
  QSpinBox *ui_duration_dd;
  QLabel *ui_duration_label;
  QCheckBox *ui_permanent;
  QTextEdit *ui_details;
  QDialogButtonBox *ui_button_box;

private Q_SLOTS:
  void onAcceptedClicked();
};

#endif // MODERATOR_DIALOG_H
