#include "MessageBox.hpp"

#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

#include "revision_utils.hpp"

namespace
{
QString decodiumMessageBoxStyleSheet()
{
  return QStringLiteral(R"(
QMessageBox {
  background-color: #111827;
  color: #E8F4FD;
  border: 1px solid #294766;
  border-radius: 14px;
}

QLabel {
  background: transparent;
  color: #E8F4FD;
}

QLabel#qt_msgbox_label {
  color: #F4FAFF;
  font-size: 18px;
  font-weight: 700;
  min-width: 360px;
}

QLabel#qt_msgbox_informativelabel {
  color: #A9C7DB;
  font-size: 14px;
  min-width: 360px;
}

QTextEdit {
  background-color: #0A0F1A;
  color: #E8F4FD;
  border: 1px solid #294766;
  border-radius: 8px;
  padding: 8px;
}

QPushButton {
  background-color: #162235;
  color: #E8F4FD;
  border: 1px solid #00D4FF;
  border-radius: 12px;
  padding: 10px 20px;
  min-width: 100px;
  min-height: 42px;
  font-size: 14px;
  font-weight: 600;
}

QPushButton:hover {
  background-color: #1D3047;
}

QPushButton:pressed {
  background-color: #14384A;
}

QPushButton:default {
  background-color: #14384A;
  border-color: #00D4FF;
}
)");
}

void polishMessageBox(MessageBox& box)
{
  box.setWindowTitle(program_title());
  box.setStyleSheet(decodiumMessageBoxStyleSheet());
  box.setMinimumWidth(520);

  if (auto * label = box.findChild<QLabel *> (QStringLiteral("qt_msgbox_label")); label) {
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
  }

  if (auto * label = box.findChild<QLabel *> (QStringLiteral("qt_msgbox_informativelabel")); label) {
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
  }

  if (auto * detail = box.findChild<QTextEdit *> (); detail) {
    detail->setMinimumWidth(460);
  }

  if (auto * buttonBox = box.findChild<QDialogButtonBox *> (); buttonBox) {
    buttonBox->setCenterButtons(false);
  }
}
}

MessageBox::MessageBox (QWidget * parent)
  : QMessageBox {parent}
{
  setWindowTitle (program_title ());
  setStyleSheet (decodiumMessageBoxStyleSheet ());
}

MessageBox::MessageBox (Icon icon, QString const& text, StandardButtons buttons
                        , QWidget * parent, Qt::WindowFlags flags)
  : QMessageBox {icon, program_title (), text, buttons, parent, flags}
{
  setStyleSheet (decodiumMessageBoxStyleSheet ());
}

void MessageBox::about_message (QWidget * parent, QString const& text)
{
  MessageBox mb {Information, text, Ok, parent};
  polishMessageBox (mb);
  mb.exec ();
}

void MessageBox::about_Qt_message (QWidget * parent)
{
  QMessageBox::aboutQt (parent, program_title ());
}

namespace
{
  QMessageBox::StandardButton show_it (QWidget * parent, MessageBox::Icon icon
                                       , QString const& text
                                       , QString const& informative
                                       , QString const& detail
                                       , MessageBox::StandardButtons buttons
                                       , MessageBox::StandardButton default_button)
  {
    MessageBox mb {icon, text, MessageBox::NoButton, parent};
    QDialogButtonBox * button_box = mb.findChild<QDialogButtonBox *> ();
    Q_ASSERT (button_box);

    uint mask = MessageBox::FirstButton;
    while (mask <= MessageBox::LastButton) {
      uint sb = buttons & mask;
      mask <<= 1;
      if (!sb)
        continue;
      QPushButton * button = mb.addButton (static_cast<MessageBox::StandardButton> (sb));
      // Choose the first accept role as the default
      if (mb.defaultButton ())
        continue;
      if ((default_button == MessageBox::NoButton
           && button_box->buttonRole (button) == QDialogButtonBox::AcceptRole)
          || (default_button != MessageBox::NoButton
              && sb == static_cast<uint> (default_button)))
        mb.setDefaultButton (button);
    }
    mb.setInformativeText (informative);
    mb.setDetailedText (detail);
    polishMessageBox (mb);
    if (mb.exec() == -1)
      return MessageBox::Cancel;
    return mb.standardButton (mb.clickedButton ());
  }
}

auto MessageBox::information_message (QWidget * parent, QString const& text
                                      , QString const& informative
                                      , QString const& detail
                                      , StandardButtons buttons
                                      , StandardButton default_button) -> StandardButton
{
  return show_it (parent, Information, text, informative, detail, buttons, default_button);
}

auto MessageBox::query_message (QWidget * parent, QString const& text
                                , QString const& informative
                                , QString const& detail
                                , StandardButtons buttons
                                , StandardButton default_button) -> StandardButton
{
  return show_it (parent, Question, text, informative, detail, buttons, default_button);
}

auto MessageBox::warning_message (QWidget * parent, QString const& text
                                  , QString const& informative
                                  , QString const& detail
                                  , StandardButtons buttons
                                  , StandardButton default_button) -> StandardButton
{
  return show_it (parent, Warning, text, informative, detail, buttons, default_button);
}

auto MessageBox::critical_message (QWidget * parent, QString const& text
                                   , QString const& informative
                                   , QString const& detail
                                   , StandardButtons buttons
                                   , StandardButton default_button) -> StandardButton
{
  return show_it (parent, Critical, text, informative, detail, buttons, default_button);
}
