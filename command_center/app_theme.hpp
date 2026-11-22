#pragma once

#include <QVariantMap>

class QApplication;

namespace app_theme
{
const QVariantMap & colors();
void apply(QApplication & app, bool light = false);
}
