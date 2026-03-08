#pragma once
/**
 * ConsolePanel.h — Bottom dock: log output + command entry
 */

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDateTime>

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget* parent = nullptr);

    void log(const QString& msg);
    void logError(const QString& msg);

signals:
    void commandSubmitted(const QString& cmd);

private slots:
    void onReturn();

private:
    QTextEdit*  output_;
    QLineEdit*  input_;
};
