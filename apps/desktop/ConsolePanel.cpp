#include "ConsolePanel.h"

ConsolePanel::ConsolePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    output_ = new QTextEdit;
    output_->setReadOnly(true);
    output_->setFontFamily("Consolas, 'Courier New', monospace");
    output_->setFontPointSize(9);
    layout->addWidget(output_);

    input_ = new QLineEdit;
    input_->setPlaceholderText(tr("Enter command…"));
    connect(input_, &QLineEdit::returnPressed, this, &ConsolePanel::onReturn);
    layout->addWidget(input_);

    log("VSEPR Desktop — Console ready");
}

void ConsolePanel::log(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    output_->append(QString("<span style='color:#808080'>[%1]</span> %2").arg(ts, msg.toHtmlEscaped()));
}

void ConsolePanel::logError(const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    output_->append(QString("<span style='color:#808080'>[%1]</span> <span style='color:#e06060'>%2</span>").arg(ts, msg.toHtmlEscaped()));
}

void ConsolePanel::onReturn()
{
    QString cmd = input_->text().trimmed();
    if (cmd.isEmpty()) return;

    log(QString("<b>&gt; %1</b>").arg(cmd.toHtmlEscaped()));
    input_->clear();

    emit commandSubmitted(cmd);
}
