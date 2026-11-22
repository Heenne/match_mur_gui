#include "ui_preferences.hpp"

#include <QApplication>
#include <QLibraryInfo>

#include "app_theme.hpp"

namespace
{
QString settings_path()
{
  const auto override_path = qEnvironmentVariable("MUR_COMMAND_CENTER_SETTINGS");
  return override_path.isEmpty() ?
    QSettings(QSettings::IniFormat, QSettings::UserScope, "MATCH", "MuRCommandCenter").fileName() :
    override_path;
}
}

UiPreferences::UiPreferences(const QString & translation_directory, QObject * parent)
: QObject(parent), settings_(settings_path(), QSettings::IniFormat)
{
  light_theme_ = settings_.value("appearance/theme", "dark").toString() == "light";
  app_theme::apply(*qApp, light_theme_);
  german_.load("command_center_de", translation_directory);
  const auto qt_translations = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
  qt_base_german_.load("qtbase_de", qt_translations);
  qt_quick_german_.load("qtdeclarative_de", qt_translations);
  setLanguage(settings_.value("appearance/language", "en").toString());
}

UiPreferences::~UiPreferences()
{
  if (language_ != "de") return;
  qApp->removeTranslator(&german_);
  qApp->removeTranslator(&qt_quick_german_);
  qApp->removeTranslator(&qt_base_german_);
}

void UiPreferences::setLightTheme(bool light)
{
  if (light == light_theme_) return;
  light_theme_ = light;
  settings_.setValue("appearance/theme", light ? "light" : "dark");
  settings_.sync();
  app_theme::apply(*qApp, light_theme_);
  emit themeChanged();
}

void UiPreferences::setLanguage(const QString & language)
{
  if (language == language_ || (language != "en" && language != "de")) return;
  if (language == "de") {
    if (german_.isEmpty()) {
      qWarning("German Command Center translations could not be loaded");
      return;
    }
    qApp->installTranslator(&qt_base_german_);
    qApp->installTranslator(&qt_quick_german_);
    qApp->installTranslator(&german_);
  } else {
    qApp->removeTranslator(&german_);
    qApp->removeTranslator(&qt_quick_german_);
    qApp->removeTranslator(&qt_base_german_);
  }
  language_ = language;
  settings_.setValue("appearance/language", language_);
  settings_.sync();
  emit languageChanged();
}
