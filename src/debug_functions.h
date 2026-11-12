#ifndef DEBUG_FUNCTIONS_H
#define DEBUG_FUNCTIONS_H

#include <QMessageBox>
#include <QString>

void call_error(QString message);
void call_notice(QString message, int seconds = 2);

#endif // DEBUG_FUNCTIONS_H
