#include "ConsolePanel.h"

// ============================================================================
// CommandLineEdit — history-aware input
// ============================================================================
CommandLineEdit::CommandLineEdit(QWidget* parent) : QLineEdit(parent) {}

void CommandLineEdit::addToHistory(const QString& cmd)
{
    if (!cmd.isEmpty() && (history_.isEmpty() || history_.last() != cmd))
        history_.append(cmd);
    histIdx_ = -1;
}

void CommandLineEdit::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Up) {
        if (history_.isEmpty()) return;
        if (histIdx_ < history_.size()-1) ++histIdx_;
        setText(history_[history_.size()-1-histIdx_]);
    } else if (e->key() == Qt::Key_Down) {
        if (histIdx_ > 0) { --histIdx_; setText(history_[history_.size()-1-histIdx_]); }
        else { histIdx_=-1; clear(); }
    } else {
        QLineEdit::keyPressEvent(e);
    }
}

// ============================================================================
// ConsolePanel
// ============================================================================
ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    output_ = new QTextEdit;
    output_->setReadOnly(true);
    output_->setFont(QFont("Consolas, 'Courier New', monospace", 9));
    output_->document()->setMaximumBlockCount(2000);
    layout->addWidget(output_);

    input_ = new CommandLineEdit;
    input_->setPlaceholderText(tr("Enter command… (relax | md | sp | reset | help)"));
    connect(input_, &QLineEdit::returnPressed, this, &ConsolePanel::onReturn);
    layout->addWidget(input_);

    log("VSEPR Desktop — Console ready.");
    log("Commands: <b>relax</b>, <b>md</b>, <b>sp</b>, <b>reset</b>, <b>help</b>");
}

QString ConsolePanel::timestamp()
{
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

void ConsolePanel::append(const QString& html)
{
    output_->append(html);
    output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
}

void ConsolePanel::log(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#c8c8c8'>%2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logSuccess(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#50c878'>✓ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logWarning(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#f0a830'>⚠ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logError(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#e05050'>✗ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::clear()  { output_->clear(); }

void ConsolePanel::onReturn()
{
    QString cmd = input_->text().trimmed();
    if (cmd.isEmpty()) return;
    append(QString("<span style='color:#4a90d9'>&gt; %1</span>")
        .arg(cmd.toHtmlEscaped()));
    input_->addToHistory(cmd);
    input_->clear();
    emit commandSubmitted(cmd);
}
