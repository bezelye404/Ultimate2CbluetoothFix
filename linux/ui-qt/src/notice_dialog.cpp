#include "u2c/uiqt/notice_dialog.h"

#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "u2c/ui/palette.h"
#include "u2c/uiqt/colors.h"
#include "u2c/uiqt/flat_button.h"

namespace u2c::uiqt {

namespace {

using ui::ColorId;

QLabel* make_label(const QString& text, int pixel_size, ColorId color, QFont::Weight weight, QWidget* parent) {
    QLabel* l = new QLabel(text, parent);
    QFont f = l->font();
    f.setPixelSize(pixel_size);
    f.setWeight(weight);
    l->setFont(f);
    l->setWordWrap(true);
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, qcolor(color));
    l->setPalette(p);
    return l;
}

}  // namespace

NoticeDialog::NoticeDialog(const Catalog& catalog, ui::UiNotice notice, const std::string& detail, ui::Distro distro, QWidget* parent)
    : QDialog(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString::fromStdString(catalog.text("NoticeTitle")));
    setFixedWidth(640);
    QPalette dp = palette();
    dp.setColor(QPalette::Window, qcolor(ColorId::WindowBg));
    setPalette(dp);
    setAutoFillBackground(true);

    const ui::NoticeContent content = ui::notice_content(notice, distro);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    layout->addWidget(make_label(QString::fromStdString(catalog.text("NoticeTitle")), 20, ColorId::TextPrimary, QFont::DemiBold, this));
    layout->addWidget(make_label(QString::fromStdString(catalog.format(content.body_key, {{"detail", detail}})), 15, ColorId::TextSecondary,
                                 QFont::Normal, this));

    QString commands_text;
    if (!content.commands.empty()) {
        layout->addWidget(make_label(QString::fromStdString(catalog.text("NoticeHowToFix")), 15, ColorId::TextSecondary, QFont::Normal, this));
        for (size_t i = 0; i < content.commands.size(); ++i) {
            if (i) commands_text += '\n';
            commands_text += QString::fromStdString(content.commands[i]);
        }
        auto* box = new QPlainTextEdit(this);
        box->setReadOnly(true);
        box->setPlainText(commands_text);
        box->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        box->setFrameStyle(QFrame::NoFrame);
        QFont mono(QStringLiteral("monospace"));
        mono.setPixelSize(13);
        box->setFont(mono);
        QPalette bp = box->palette();
        bp.setColor(QPalette::Base, qcolor(ColorId::CardBg));
        bp.setColor(QPalette::Text, qcolor(ColorId::TextPrimary));
        box->setPalette(bp);
        box->setFixedHeight(static_cast<int>(content.commands.size()) * 46 + 14);
        layout->addWidget(box);
        if (content.commands_unverified)
            layout->addWidget(make_label(QString::fromStdString(catalog.text("NoticeCommandsUnverified")), 12, ColorId::TextMuted, QFont::Normal, this));
    } else {
        layout->addWidget(make_label(QString::fromStdString(catalog.text("NoticeNoCommand")), 15, ColorId::TextSecondary, QFont::Normal, this));
    }

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    if (!commands_text.isEmpty()) {
        auto* copy = new FlatButton(this);
        copy->setFixedSize(130, 34);
        copy->setText(QString::fromStdString(catalog.text("NoticeCopy")));
        const QString copied = QString::fromStdString(catalog.text("NoticeCopied"));
        const QString copy_text = copy->text();
        connect(copy, &QAbstractButton::clicked, this, [copy, commands_text, copied, copy_text] {
            QGuiApplication::clipboard()->setText(commands_text);
            copy->setText(copied);
            QTimer::singleShot(1500, copy, [copy, copy_text] { copy->setText(copy_text); });
        });
        buttons->addWidget(copy);
    }
    auto* close = new FlatButton(this, FlatButton::Style::Accent);
    close->setFixedSize(110, 34);
    close->setText(QString::fromStdString(catalog.text("NoticeClose")));
    connect(close, &QAbstractButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    adjustSize();
}

}  // namespace u2c::uiqt
