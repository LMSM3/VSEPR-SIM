#include "ConsolePanel.h"

// ============================================================================
// HistoryLineEdit
// ============================================================================

HistoryLineEdit::HistoryLineEdit(QWidget* parent)
    : QLineEdit(parent)
{}

void HistoryLineEdit::addToHistory(const QString& cmd)
{
    if (cmd.isEmpty()) return;
    // Avoid consecutive duplicates
    if (!history_.isEmpty() && history_.last() == cmd)
        return;
    history_.append(cmd);
    // Cap history at 200 entries
    if (history_.size() > 200)
        history_.removeFirst();
    history_pos_ = -1;
    pending_.clear();
}

void HistoryLineEdit::keyPressEvent(QKeyEvent* e)
{
    if (history_.isEmpty()) {
        QLineEdit::keyPressEvent(e);
        return;
    }

    if (e->key() == Qt::Key_Up) {
        if (history_pos_ == -1) {
            pending_ = text();          // save current draft
            history_pos_ = history_.size() - 1;
        } else if (history_pos_ > 0) {
            --history_pos_;
        }
        setText(history_.at(history_pos_));
        return;
    }

    if (e->key() == Qt::Key_Down) {
        if (history_pos_ == -1) return;
        ++history_pos_;
        if (history_pos_ >= history_.size()) {
            history_pos_ = -1;
            setText(pending_);
        } else {
            setText(history_.at(history_pos_));
        }
        return;
    }

    // Any other key: reset browsing so the next Up starts from the latest entry
    history_pos_ = -1;
    QLineEdit::keyPressEvent(e);
}

// ============================================================================
// ConsolePanel
// ============================================================================

ConsolePanel::ConsolePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    output_ = new QTextEdit;
    output_->setReadOnly(true);
    output_->setFontFamily("Consolas");
    output_->setFontPointSize(9);
    output_->document()->setMaximumBlockCount(4000);
    layout->addWidget(output_);

    input_ = new HistoryLineEdit;
    input_->setPlaceholderText(tr("Enter command\xe2\x80\xa6"));
    connect(input_, &QLineEdit::returnPressed, this, &ConsolePanel::onReturn);
    layout->addWidget(input_);

    log("VSEPR Desktop \xe2\x80\x94 Console ready.  Type 'help' for commands.");
}

// ----------------------------------------------------------------------------
// Private helper — stamped HTML line
// ----------------------------------------------------------------------------

void ConsolePanel::appendHtml(const QString& ts, const QString& color,
                               const QString& body)
{
    output_->append(
        QString("<span style='color:#606060'>[%1]</span> "
                "<span style='color:%2'>%3</span>")
            .arg(ts, color, body));
}

// ----------------------------------------------------------------------------
// Public logging API
// ----------------------------------------------------------------------------

void ConsolePanel::log(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendHtml(ts, "#d4d4d4", msg.toHtmlEscaped());
}

void ConsolePanel::logError(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendHtml(ts, "#e06060",
               QString("<b>%1</b>").arg(msg.toHtmlEscaped()));
}

void ConsolePanel::logResult(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendHtml(ts, "#4ec9b0", msg.toHtmlEscaped());
}

void ConsolePanel::clearLog()
{
    output_->clear();
}

// ----------------------------------------------------------------------------
// Slot — return key in input field
// ----------------------------------------------------------------------------

void ConsolePanel::onReturn()
{
    QString cmd = input_->text().trimmed();
    if (cmd.isEmpty()) return;

    // Echo in bold italic with a distinct command colour
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendHtml(ts, "#9cdcfe",
               QString("<b><i>&gt; %1</i></b>").arg(cmd.toHtmlEscaped()));

    input_->addToHistory(cmd);
    input_->clear();

    emit commandSubmitted(cmd);
}
