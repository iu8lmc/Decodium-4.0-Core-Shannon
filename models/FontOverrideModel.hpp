#ifndef FONT_OVERRIDE_MODEL_HPP_
#define FONT_OVERRIDE_MODEL_HPP_

#include <QIdentityProxyModel>
#include <QFont>

// fix up font display as header font changes don't currently work
// from views (I think fixed in Qt 5.11.1)
class FontOverrideModel final
  : public QIdentityProxyModel
{
public:
  FontOverrideModel (QObject * parent = nullptr) : QIdentityProxyModel {parent} {}
  void set_font (QFont const& font) {font_ = font;}
  QVariant data (QModelIndex const& index, int role) const override
  {
    if (Qt::FontRole == role) return font_;
    return QIdentityProxyModel::data (index, role);
  }
  QVariant headerData (int section, Qt::Orientation orientation, int role) const override
  {
    if (Qt::FontRole == role) return font_;
    return QIdentityProxyModel::headerData (section, orientation, role);
  }
private:
  QFont font_;
};

#endif
