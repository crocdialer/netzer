#include <mutex>
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>
#include "netzer/Serial.hpp"

namespace netzer
{
namespace
{
std::mutex g_mutex;
std::map<std::string, SerialWeakPtr> g_connected_devices;
}

struct SerialImpl
{
    std::string m_device_name;
    boost::asio::serial_port m_serial_port;
    Serial::connection_cb_t m_connect_cb, m_disconnect_cb;
    Serial::receive_cb_t m_receive_cb;
    std::vector<uint8_t> m_rec_buffer = std::vector<uint8_t>(512);
//    CircularBuffer<uint8_t> m_buffer{512 * (1 << 10)};
    std::vector<uint8_t> m_buffer = std::vector<uint8_t>(512 * (1 << 10));
    std::mutex m_mutex;

    SerialImpl(boost::asio::io_service &io, Serial::receive_cb_t rec_cb) :
            m_serial_port(io),
            m_receive_cb(std::move(rec_cb)){}
};

///////////////////////////////////////////////////////////////////////////////

SerialPtr Serial::create(io_service_t &io, receive_cb_t cb)
{
    auto ret = SerialPtr(new Serial(io, std::move(cb)));
    return ret;
}

///////////////////////////////////////////////////////////////////////////////

Serial::Serial(io_service_t &io, receive_cb_t cb) :
        m_impl(new SerialImpl(io, std::move(cb)))
{

}

///////////////////////////////////////////////////////////////////////////////

Serial::~Serial()
{
    close();
}

///////////////////////////////////////////////////////////////////////////////

std::map<std::string, SerialPtr> Serial::connected_devices()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::map<std::string, SerialPtr> ret;

    for(auto &p: g_connected_devices)
    {
        auto ptr = p.second.lock();

        if(ptr){ ret[p.first] = ptr; }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////

//std::vector<std::string> Serial::device_list(const std::set<std::string> &the_patterns)
//{
//    std::vector<std::string> ret;
//    std::set<std::string> search_patterns = {"tty.usb", "ttyACM"};
//    if(!the_patterns.empty()){ search_patterns = the_patterns; }
//
//    for(const auto &path: fs::get_directory_entries("/dev"))
//    {
//        for(const auto &p: search_patterns)
//        {
//            if(path.find(p) != std::string::npos)
//            {
//                ret.push_back(path);
//                break;
//            }
//        }
//    }
//    return ret;
//}

///////////////////////////////////////////////////////////////////////////////

bool Serial::open()
{
    bool ret = false;
    //for(const auto &d: device_list())
    //{
    //    if(open(d))
    //    {
    //        ret = true;
    //        break;
    //    }
    //}
    return ret;
}

///////////////////////////////////////////////////////////////////////////////

bool Serial::open(const std::string &the_name, int the_baudrate)
{
    if(is_open())
    {
        if(m_impl->m_device_name == the_name){ return false; }
        close();
    }
    boost::asio::serial_port_base::baud_rate br(the_baudrate);
    boost::asio::serial_port_base::flow_control flow_control;
    boost::asio::serial_port_base::parity parity;
    boost::asio::serial_port_base::stop_bits stop_bits;
    boost::asio::serial_port_base::character_size char_size;

    try
    {
        m_impl->m_serial_port.open(the_name);
        m_impl->m_serial_port.set_option(br);
        m_impl->m_serial_port.set_option(flow_control);
        m_impl->m_serial_port.set_option(parity);
        m_impl->m_serial_port.set_option(stop_bits);
        m_impl->m_serial_port.set_option(char_size);
        m_impl->m_device_name = the_name;

        std::lock_guard<std::mutex> lock(g_mutex);
        g_connected_devices[the_name] = shared_from_this();
        async_read_bytes();
        if(m_impl->m_connect_cb){ m_impl->m_connect_cb(shared_from_this()); }
        return true;
    }
    catch(boost::system::system_error&){ return false; }
}

///////////////////////////////////////////////////////////////////////////////

void Serial::close()
{
    if(is_open())
    {
        try{ m_impl->m_serial_port.close(); }
        catch(boost::system::system_error &){}
    }
}

///////////////////////////////////////////////////////////////////////////////

bool Serial::is_open() const
{
    return m_impl->m_serial_port.is_open();
}

///////////////////////////////////////////////////////////////////////////////

size_t Serial::read_bytes(void *buffer, size_t sz)
{
    std::unique_lock<std::mutex> lock(m_impl->m_mutex);
    size_t num_bytes = std::min(m_impl->m_buffer.size(), sz);
    std::copy(m_impl->m_buffer.begin(), m_impl->m_buffer.begin() + (int)num_bytes,
              (uint8_t *) buffer);
    auto tmp = std::vector<uint8_t>(m_impl->m_buffer.begin() + (int)num_bytes,
                                    m_impl->m_buffer.end());
    m_impl->m_buffer.assign(tmp.begin(), tmp.end());
    return num_bytes;
}

///////////////////////////////////////////////////////////////////////////////

size_t Serial::write_bytes(const void *buffer, size_t sz)
{
//        try{ return boost::asio::write(m_impl->m_serial_port, boost::asio::buffer(buffer, sz)); }
//        catch(boost::system::system_error &e){ LOG_WARNING << e.what(); }
    async_write_bytes(buffer, sz);
    return sz;
}

///////////////////////////////////////////////////////////////////////////////

void Serial::async_read_bytes()
{
    auto weak_self = std::weak_ptr<Serial>(shared_from_this());
    auto impl_cp = m_impl;

    m_impl->m_serial_port.async_read_some(boost::asio::buffer(m_impl->m_rec_buffer),
                                          [weak_self, impl_cp](const boost::system::error_code &error,
                                                               std::size_t bytes_transferred)
                                          {
                                              auto self = weak_self.lock();

                                              if(!error)
                                              {
                                                  if(bytes_transferred)
                                                  {
                                                      if(self && impl_cp->m_receive_cb)
                                                      {
                                                          std::vector<uint8_t> datavec(impl_cp->m_rec_buffer.begin(),
                                                                                       impl_cp->m_rec_buffer.begin() +
                                                                                       bytes_transferred);
                                                          impl_cp->m_receive_cb(self, datavec);
                                                      }
                                                      else
                                                      {
                                                          std::unique_lock<std::mutex> lock(impl_cp->m_mutex);
                                                          std::copy(impl_cp->m_rec_buffer.begin(),
                                                                    impl_cp->m_rec_buffer.begin() + bytes_transferred,
                                                                    std::back_inserter(impl_cp->m_buffer));
                                                      }
                                                  }
                                                  if(self){ self->async_read_bytes(); }
                                              }
                                              else
                                              {
                                                  switch(error.value())
                                                  {
                                                      case boost::asio::error::eof:
                                                      case boost::asio::error::connection_reset:
                                                      case boost::system::errc::no_such_device_or_address:
                                                      case boost::asio::error::operation_aborted:
                                                      {
//                        LOG_TRACE_1 << "disconnected: " << impl_cp->m_device_name;
                                                          if(self && impl_cp->m_disconnect_cb)
                                                          {
                                                              auto diconnect_cb = std::move(impl_cp->m_disconnect_cb);
                                                              diconnect_cb(self);
                                                          }
                                                          std::lock_guard<std::mutex> lock(g_mutex);
                                                          g_connected_devices.erase(impl_cp->m_device_name);
                                                      }
                                                          break;

                                                      default:
//                        LOG_TRACE_2 << error.message() << " (" << error.value() << ")";
                                                          break;
                                                  }
                                              }
                                          });
}

///////////////////////////////////////////////////////////////////////////////

void Serial::async_write_bytes(const void *buffer, size_t sz)
{
    std::vector<uint8_t> bytes((uint8_t *) buffer, (uint8_t *) buffer + sz);

    boost::asio::async_write(m_impl->m_serial_port, boost::asio::buffer(bytes),
                             [bytes](const boost::system::error_code & /*error*/,
                                     std::size_t /*bytes_transferred*/)
                             {
//            if(error)
//            {
////                LOG_ERROR << error.message();
//            }
//            else if(bytes_transferred < bytes.size())
//            {
////                LOG_WARNING << "not all bytes written";
//            }
                             });
}

///////////////////////////////////////////////////////////////////////////////

size_t Serial::available() const
{
    return m_impl->m_buffer.size();
}

///////////////////////////////////////////////////////////////////////////////

std::string Serial::description() const
{
    return m_impl->m_device_name;
}

///////////////////////////////////////////////////////////////////////////////

void Serial::drain()
{
//        m_impl->m_serial_port.cancel();
    m_impl->m_buffer.clear();
//        async_read_bytes();
}

///////////////////////////////////////////////////////////////////////////////

void Serial::set_receive_cb(receive_cb_t the_cb)
{
    m_impl->m_receive_cb = std::move(the_cb);

    // we have some buffered data -> deliver it to the newly attached callback
    if(m_impl->m_receive_cb && !m_impl->m_buffer.empty())
    {
        std::unique_lock<std::mutex> lock(m_impl->m_mutex);
        m_impl->m_receive_cb(shared_from_this(), std::vector<uint8_t>(m_impl->m_buffer.begin(),
                                                                      m_impl->m_buffer.end()));
        m_impl->m_buffer.clear();
    }
}

///////////////////////////////////////////////////////////////////////////////

void Serial::set_connect_cb(connection_cb_t the_cb)
{
    m_impl->m_connect_cb = std::move(the_cb);
}

///////////////////////////////////////////////////////////////////////////////

void Serial::set_disconnect_cb(connection_cb_t the_cb)
{
    m_impl->m_disconnect_cb = std::move(the_cb);
}

///////////////////////////////////////////////////////////////////////////////
}
