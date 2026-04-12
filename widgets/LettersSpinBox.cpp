#include "LettersSpinBox.hpp"

#include <QString>
#include "moc_LettersSpinBox.cpp"

QString LettersSpinBox::textFromValue (int value) const
{
  QString text;
  do {
    auto digit = value % 26;
    value /= 26;
    text = QChar {lowercase_ ? 'a' + digit : 'A' + digit} + text;
  } while (value);
  return text;
}

int LettersSpinBox::valueFromText (QString const& text) const
{
  int value {0};
  for (int index = text.size (); index > 0; --index)
    {
      value = value * 26 + text[index - 1].toLatin1 () - (lowercase_ ? 'a' : 'A');
    }
  return value;
}
