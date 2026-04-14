#include "AbstractLogWindow.hpp"

#include <algorithm>
#include <QSettings>
#include <QObject>
#include <QString>
#include <QTableView>
#include <QHeaderView>
#include <QAction>
#include <QSqlTableModel>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QTimer>
#include "Configuration.hpp"
#include "SettingsGroup.hpp"
#include "WindowGeometryUtils.hpp"
#include "MessageBox.hpp"
#include "models/FontOverrideModel.hpp"
#include "pimpl_impl.hpp"

class AbstractLogWindow::impl final
  : public QObject
{
  Q_OBJECT

public:
  impl (AbstractLogWindow * self, QString const& settings_key, QSettings * settings
        , Configuration const * configuration)
    : self_ {self}
    , settings_key_ {settings_key}
    , settings_ {settings}
    , configuration_ {configuration}
    , log_view_ {nullptr}
  {
  }

  void delete_QSOs ();

  AbstractLogWindow * self_;
  QString settings_key_;
  QSettings * settings_;
  Configuration const * configuration_;
  QTableView * log_view_;
  FontOverrideModel model_;
};

#include "moc_AbstractLogWindow.cpp"
#include "AbstractLogWindow.moc"

namespace
{
  bool row_is_higher (QModelIndex const& lhs, QModelIndex const& rhs)
  {
    return lhs.row () > rhs.row ();
  }
}

void AbstractLogWindow::impl::delete_QSOs ()
{
  auto selection_model = log_view_->selectionModel ();
  selection_model->select (selection_model->selection (), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
  auto row_indexes = selection_model->selectedRows ();

  if (row_indexes.size ()
      && MessageBox::Yes == MessageBox::query_message (self_
                                                       , tr ("Confirm Delete")
                                                       , tr ("Are you sure you want to delete the %n "
                                                             "selected QSO(s) from the log?", ""
                                                             , row_indexes.size ())
                                                       , QString {}
                                                       , MessageBox::Yes | MessageBox::No
                                                       , MessageBox::No))
    {
      // We must work with source model indexes because we don't want row
      // removes to invalidate model indexes we haven't yet processed. We
      // achieve that by processing them in descending row order.
      for (auto& row_index : row_indexes)
        {
          row_index = model_.mapToSource (row_index);
        }

      // reverse sort by row
      std::sort (row_indexes.begin (), row_indexes.end (), row_is_higher);

      for (auto index : row_indexes)
        {
          auto row = model_.mapFromSource(index).row();
          model_.removeRow(row);
          self_->log_model_changed();
        }
    }
}


AbstractLogWindow::AbstractLogWindow (QString const& settings_key, QSettings * settings
                                      , Configuration const * configuration
                                      , QWidget * parent)
  : QWidget {parent}
  , m_ {this, settings_key, settings, configuration} {
    // when we're viewing the log by contact ID (visually, up/down chevron in the column heading),
    // when we add a contact, scroll the list to the top or bottom, depending on the sort order.
    // If the table is sorted by some other criteria, don't change anything.

    connect(&m_->model_, &QAbstractItemModel::rowsInserted, this,
            [this](QModelIndex const &parent, int first, int last) {
                (void) (parent); // UNUSED
                (void) (first);  // UNUSED
                (void) (last);   // UNUSED
                QTimer::singleShot(0, [=] {
                    // if we're sorting by the date, then show the most-recently logged contact.
                    // Otherwise, leave the scroll alone
                    auto horizontal_header = m_->log_view_->horizontalHeader ();
                    if (horizontal_header->sortIndicatorSection() == 3) { // sorting on date?
                        if (horizontal_header->sortIndicatorOrder() == Qt::AscendingOrder) {
                            // we're sorting oldes->newest, so go to bottom
                            m_->log_view_->scrollToBottom();
                        } else {
                            m_->log_view_->scrollToTop();
                        }
                    }

                });
            }
    );
}

AbstractLogWindow::~AbstractLogWindow ()
{
  SettingsGroup g {m_->settings_, m_->settings_key_};
  m_->settings_->setValue ("window/geometry", saveGeometry ());
}

void AbstractLogWindow::set_log_view (QTableView * log_view)
{
  // do this here because we know the UI must be setup before this
  SettingsGroup g {m_->settings_, m_->settings_key_};
  WindowGeometryUtils::restore_window_geometry (this, m_->settings_->value ("window/geometry").toByteArray ());
  m_->log_view_ = log_view;
  set_log_view_font (m_->configuration_->decoded_text_font ());
  log_view->setSortingEnabled (true);
  log_view->setContextMenuPolicy (Qt::ActionsContextMenu);
  log_view->setAlternatingRowColors (true);
  log_view->setSelectionBehavior (QAbstractItemView::SelectRows);
  log_view->setSelectionMode (QAbstractItemView::ExtendedSelection);
  log_view->setVerticalScrollMode (QAbstractItemView::ScrollPerPixel);
  m_->model_.setSourceModel (log_view->model ());
  log_view->setModel (&m_->model_);
  log_view->setColumnHidden (0, true); // hide the ID column
  auto horizontal_header = log_view->horizontalHeader ();

  horizontal_header->setResizeContentsPrecision (0); // visible region only
  horizontal_header->setSectionResizeMode (QHeaderView::ResizeToContents);
  horizontal_header->setSectionsMovable (true);
  horizontal_header->setSortIndicator(3, Qt::AscendingOrder); // sort by the contact datetime oldest->newest

  auto vertical_header = log_view->horizontalHeader ();
  vertical_header->setResizeContentsPrecision (0); // visible region only
  vertical_header->setSectionResizeMode (QHeaderView::ResizeToContents);

  // actions
  auto delete_action = new QAction {tr ("&Delete ..."), log_view};
  log_view->insertAction (nullptr, delete_action);
  connect (delete_action, &QAction::triggered, [this] (bool /*checked*/) {
      m_->delete_QSOs ();
    });

  // scroll to bottom, since we're showing 1-N
  log_view->scrollToBottom();
}

void AbstractLogWindow::set_log_view_font (QFont const& font)
{
  m_->log_view_->setFont (font);
  m_->log_view_->horizontalHeader ()->setFont (font);
  m_->log_view_->verticalHeader ()->setFont (font);
  m_->model_.set_font (font);
}
