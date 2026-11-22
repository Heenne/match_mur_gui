#pragma once

#include <QObject>
#include <QSettings>
#include <QTranslator>

class UiPreferences : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool lightTheme READ lightTheme WRITE setLightTheme NOTIFY themeChanged)
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
  explicit UiPreferences(const QString & translation_directory, QObject * parent = nullptr);
  ~UiPreferences() override;

  bool lightTheme() const { return light_theme_; }
  QString language() const { return language_; }
  void setLightTheme(bool light);
  void setLanguage(const QString & language);

signals:
  void themeChanged();
  void languageChanged();

private:
  QSettings settings_;
  QTranslator german_;
  QTranslator qt_base_german_;
  QTranslator qt_quick_german_;
  bool light_theme_{false};
  QString language_{"en"};
};
