#ifndef MODERATION_FUNCTIONS_H
#define MODERATION_FUNCTIONS_H

#include <QString>

#include <optional>

// Prompts the user for a moderation reason. Returns the reason when one is
// provided, otherwise std::nullopt.
std::optional<QString> call_moderator_support(QString title = QString());

#endif // MODERATION_FUNCTIONS_H
