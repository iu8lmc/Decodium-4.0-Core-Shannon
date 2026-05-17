#include "NetworkServerLookup.hpp"

#include <stdexcept>

#include <QHostInfo>
#include <QString>

namespace
{
  quint16 parse_service_port (QString const& text)
  {
    bool ok {false};
    auto const port = text.trimmed ().toUInt (&ok);
    if (!ok || port > 65535u)
      {
        throw std::runtime_error {"network server lookup error: invalid port"};
      }
    return static_cast<quint16> (port);
  }

  bool is_decimal_port (QString const& text)
  {
    auto const trimmed = text.trimmed ();
    if (trimmed.isEmpty ())
      {
        return false;
      }
    for (auto const ch : trimmed)
      {
        if (!ch.isDigit ())
          {
            return false;
          }
      }
    return true;
  }
}

std::tuple<QHostAddress, quint16>
network_server_lookup (QString query
		       , quint16 default_service_port
		       , QHostAddress default_host_address
		       , QAbstractSocket::NetworkLayerProtocol required_protocol)
{
  query = query.trimmed ();

  QHostAddress host_address {default_host_address};
  quint16 service_port {default_service_port};

  QString host_name;
  if (!query.isEmpty ())
    {
      int port_colon_index {-1};

      if ('[' == query[0])
        {
          // assume IPv6 combined address/port syntax [<address>]:<port>
          auto close_bracket_index = query.lastIndexOf (']');
          if (close_bracket_index < 0)
            {
              throw std::runtime_error {"network server lookup error: invalid bracketed host"};
            }
          host_name = query.mid (1, close_bracket_index - 1);
          port_colon_index = query.indexOf (':', close_bracket_index);
        }
      else
        {
          port_colon_index = query.lastIndexOf (':');
          if (port_colon_index >= 0)
            {
              host_name = query.left (port_colon_index);
            }
          else
            {
              if (is_decimal_port (query))
                {
                  service_port = parse_service_port (query);
                }
              else
                {
                  host_name = query;
                }
            }
        }
      host_name = host_name.trimmed ();

      if (port_colon_index >= 0)
        {
          service_port = parse_service_port (query.mid (port_colon_index + 1));
        }
    }

  if (!host_name.isEmpty ())
    {
      auto host_info = QHostInfo::fromName (host_name);
      if (host_info.addresses ().isEmpty ())
        {
          throw std::runtime_error {"network server lookup error: host name lookup failed"};
        }

      bool found {false};
      for (int i {0}; i < host_info.addresses ().size () && !found; ++i)
        {
          host_address = host_info.addresses ().at (i);
          switch (required_protocol)
            {
            case QAbstractSocket::IPv4Protocol:
            case QAbstractSocket::IPv6Protocol:
              if (required_protocol != host_address.protocol ())
                {
                  break;
                }
              // fall through
            case QAbstractSocket::AnyIPProtocol:
              found = true;
              break;

            default:
              throw std::runtime_error {"network server lookup error: invalid required protocol"};
            }
        }
      if (!found)
        {
          throw std::runtime_error {"network server lookup error: no suitable host address found"};
        }
    }

  return std::make_tuple (host_address, service_port);
}
