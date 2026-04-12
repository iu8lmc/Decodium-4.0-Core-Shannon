#ifndef CALLSIGN_DELEGATE_HPP_
#define CALLSIGN_DELEGATE_HPP_

#include <QStyledItemDelegate>

class QValidator;

//
// Class CallsignDelegate
//
//	Item delegate for editing a callsign
//
class CallsignDelegate final
  : public QStyledItemDelegate
{
public:
  explicit CallsignDelegate (QObject * parent = nullptr);
  QWidget * createEditor (QWidget * parent, QStyleOptionViewItem const&, QModelIndex const&) const override;
  void setEditorData (QWidget * editor, QModelIndex const&) const override;
  void setModelData (QWidget * editor, QAbstractItemModel *, QModelIndex const&) const override;
  void updateEditorGeometry (QWidget * editor, QStyleOptionViewItem const& option, QModelIndex const&) const override
  {
    editor->setGeometry (option.rect);
  }

private:
  QScopedPointer<QValidator> validator_;
};

#endif
