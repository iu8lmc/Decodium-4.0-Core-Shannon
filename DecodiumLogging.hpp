#ifndef DECODIUM_LOGGING_HPP__
#define DECODIUM_LOGGING_HPP__

#include <QtGlobal>

//
// Class DecodiumLogging - wraps application specific logging
//
class DecodiumLogging final
{
public:
  explicit DecodiumLogging ();
  ~DecodiumLogging ();

  DecodiumLogging (DecodiumLogging const&) = delete;
  DecodiumLogging& operator= (DecodiumLogging const&) = delete;

private:
  QtMessageHandler previous_qt_message_handler_ {nullptr};
};

#endif
