#include "CallsignDelegate.hpp"

#include <QLineEdit>

#include "validators/CallsignValidator.hpp"

CallsignDelegate::CallsignDelegate (QObject * parent)
  : QStyledItemDelegate {parent}
  , validator_ {new CallsignValidator}
{
}

QWidget * CallsignDelegate::createEditor (QWidget * parent, QStyleOptionViewItem const&
                                          , QModelIndex const&) const
{
  auto * editor = new QLineEdit {parent};
  editor->setFrame (false);
  editor->setValidator (validator_.data ());
  return editor;
}

void CallsignDelegate::setEditorData (QWidget * editor, QModelIndex const& index) const
{
  static_cast<QLineEdit *> (editor)->setText (index.model ()->data (index, Qt::EditRole).toString ());
}

void CallsignDelegate::setModelData (QWidget * editor, QAbstractItemModel * model, QModelIndex const& index) const
{
  model->setData (index, static_cast<QLineEdit *> (editor)->text (), Qt::EditRole);
}

