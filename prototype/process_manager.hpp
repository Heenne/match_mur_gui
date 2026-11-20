#pragma once

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QVariantList>

class ProcessManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList processes READ processes NOTIFY changed)
  Q_PROPERTY(QString log READ log NOTIFY logChanged)

public:
  explicit ProcessManager(QObject * parent = nullptr);
  ~ProcessManager() override;

  QVariantList processes() const;
  QString log() const { return log_; }
  Q_INVOKABLE bool startSimulation(const QString & robot);
  Q_INVOKABLE bool startGazeboView();
  Q_INVOKABLE bool startProcess(const QString & name, const QString & program,
    const QStringList & arguments);
  Q_INVOKABLE void stopProcess(const QString & name);
  Q_INVOKABLE void stopAll();

signals:
  void changed();
  void logChanged();

private:
  struct ManagedProcess {
    QProcess * process{};
    QString name;
    QString state{"Stopped"};
    QStringList lines;
    int exit_code{-1};
    bool stopping{false};
  };
  void appendOutput(ManagedProcess * managed, const QByteArray & bytes, bool stderr_stream);
  void setState(ManagedProcess * managed, const QString & state);
  void appendLog(const QString & line);
  QHash<QString, ManagedProcess *> managed_;
  QString log_;
};
