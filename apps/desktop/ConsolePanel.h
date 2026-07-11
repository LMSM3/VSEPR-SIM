#pragma once
/**
 * ConsolePanel.h — Bottom dock: log output + command entry
 *
 * Colour scheme:
 *   Gray timestamp         [hh:mm:ss]
 *   White                  info messages
 *   Red                    errors
 *   Cyan/teal              results (successful computation outputs)
 *   Bold italic            echoed commands
 *
 * Command history:
 *   Up / Down arrow keys   navigate previously submitted commands.
 */

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDateTime>
#include <QKeyEvent>
#include <QStringList>

class HistoryLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit HistoryLineEdit(QWidget* parent = nullptr);
    void addToHistory(const QString& cmd);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    QStringList history_;
    int         history_pos_ = -1;  // -1 = current (not browsing)
    QString     pending_;           // text saved before browsing
};

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget* parent = nullptr);

    void log(const QString& msg);        // white info text
    void logError(const QString& msg);   // red error text
    void logResult(const QString& msg);  // cyan result text
    void clearLog();

signals:
    void commandSubmitted(const QString& cmd);

private slots:
    void onReturn();

private:
    QTextEdit*       output_;
    HistoryLineEdit* input_;

    void appendHtml(const QString& ts, const QString& color, const QString& body);
};
