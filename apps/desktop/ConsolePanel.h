#pragma once
/**
 * ConsolePanel.h — Bottom dock: timestamped log + command input
 *
 * Color coding:
 *   info    — default (light gray)
 *   success — green
 *   warning — amber
 *   error   — red
 *
 * History: up/down arrows navigate previous commands.
 */

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDateTime>
#include <QKeyEvent>
#include <QStringList>

class CommandLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit CommandLineEdit(QWidget* parent = nullptr);
    void addToHistory(const QString& cmd);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    QStringList history_;
    int         histIdx_ = -1;
};

// ---------------------------------------------------------------------------

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget* parent = nullptr);

    void log(const QString& msg);
    void logSuccess(const QString& msg);
    void logWarning(const QString& msg);
    void logError(const QString& msg);
    void clear();

signals:
    void commandSubmitted(const QString& cmd);

private slots:
    void onReturn();

private:
    QTextEdit*       output_;
    CommandLineEdit* input_;

    void append(const QString& html);
    static QString timestamp();
};
