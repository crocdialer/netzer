
#include <chrono>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include "netzer/networking.hpp"

#if defined(unix) || defined(__unix__) || defined(__unix)

#include <ifaddrs.h>

#endif

namespace netzer
{

using namespace boost::asio::ip;
using namespace std::chrono;

using duration_t = std::chrono::duration<double>;

///////////////////////////////////////////////////////////////////////////////////////////////////

char const *const UNKNOWN_IP = "0.0.0.0";

#if defined(unix) || defined(__unix__) || defined(__unix)

std::string local_ip(bool ipV6)
{
    std::string ret = UNKNOWN_IP;
    std::set<std::string> ip_set;

    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;
    void *tmpAddrPtr = nullptr;

    getifaddrs(&ifAddrStruct);

    for(ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if(!ifa->ifa_addr){ continue; }

        // check it is IP4
        if(!ipV6 && ifa->ifa_addr->sa_family == AF_INET)
        {
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
//            LOG_INFO << format("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
            ip_set.insert(addressBuffer);
        }
            // check it is IP6
        else if(ipV6 && ifa->ifa_addr->sa_family == AF_INET6)
        {
            // is a valid IP6 Address
            tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
//            LOG_INFO << format("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
            ip_set.insert(addressBuffer);
        }
    }
    if(ifAddrStruct){ freeifaddrs(ifAddrStruct); }
    ip_set.erase("127.0.0.1");
    ip_set.erase("127.0.1.1");
    if(!ip_set.empty()){ ret = *ip_set.begin(); }
    return ret;
}

#elif defined(_WIN32)
std::string local_ip(bool ipV6)
{
    std::string ret = "unknown_ip";
    std::set<std::string> ip_set;

    try
    {
        boost::asio::io_service io;
        tcp::resolver resolver(io);
        tcp::resolver::query query(ipV6 ? tcp::v6() : tcp::v4(), host_name(), "");
        tcp::resolver::iterator it = resolver.resolve(query), end;

        for(; it != end; ++it)
        {
            const tcp::endpoint &endpoint = *it;
            ip_set.insert(endpoint.address().to_string());
        }
        ip_set.erase("127.0.1.1");
    } catch(std::exception&){}
    if(!ip_set.empty()) { ret = *ip_set.begin(); }
    return ret;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

void send_tcp(const std::string &str, const std::string &ip_string, uint16_t port)
{
    send_tcp(std::vector<uint8_t>(str.begin(), str.end()), ip_string, port);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void send_tcp(const std::vector<uint8_t> &bytes,
              const std::string &ip_string, uint16_t port)
{
    try
    {
        boost::asio::io_service io_service;
        tcp::socket s(io_service);
        tcp::resolver resolver(io_service);
        boost::asio::connect(s, resolver.resolve({ip_string, std::to_string(port)}));
        boost::asio::write(s, boost::asio::buffer(bytes));
    }
    catch(std::exception&){}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_connection_ptr async_send_tcp(boost::asio::io_service &io_service,
                                  const std::string &str,
                                  const std::string &ip,
                                  uint16_t port)
{
    return async_send_tcp(io_service, std::vector<uint8_t>(str.begin(), str.end()), ip, port);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_connection_ptr async_send_tcp(boost::asio::io_service &the_io_service,
                                  const std::vector<uint8_t> &bytes,
                                  const std::string &the_ip,
                                  uint16_t the_port)
{
    auto con = tcp_connection::create(the_io_service, the_ip, the_port);
    con->set_connect_cb([bytes](ConnectionPtr the_uart)
                        {
                            the_uart->write_bytes(&bytes[0], bytes.size());
                        });
    return con;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void send_udp(const std::vector<uint8_t> &bytes,
              const std::string &ip_string, uint16_t port)
{
    try
    {
        boost::asio::io_service io_service;

        udp::resolver resolver(io_service);
        udp::resolver::query query(udp::v4(), ip_string, std::to_string(port));
        udp::endpoint receiver_endpoint = *resolver.resolve(query);

        udp::socket socket(io_service, udp::v4());
        socket.send_to(boost::asio::buffer(bytes), receiver_endpoint);
    }
    catch(std::exception&){}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void async_send_udp(boost::asio::io_service &io_service,
                    const std::string &str,
                    const std::string &ip,
                    uint16_t port)
{
    async_send_udp(io_service, std::vector<uint8_t>(str.begin(), str.end()), ip, port);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void async_send_udp(boost::asio::io_service &io_service, const std::vector<uint8_t> &bytes,
                    const std::string &ip_string, uint16_t port)
{
    try
    {
        auto socket_ptr = std::make_shared<udp::socket>(io_service, udp::v4());
        auto resolver_ptr = std::make_shared<udp::resolver>(io_service);
        udp::resolver::query query(udp::v4(), ip_string, std::to_string(port));

        resolver_ptr->async_resolve(query, [socket_ptr, resolver_ptr, ip_string, bytes]
                (const boost::system::error_code &ec,
                 udp::resolver::iterator end_point_it)
        {
            if(!ec)
            {
                socket_ptr->async_send_to(boost::asio::buffer(bytes), *end_point_it,
                                          [socket_ptr, bytes](const boost::system::error_code &/*error*/,
                                                              std::size_t /*bytes_transferred*/)
                                          {
//                                              if(error){ spdlog::error(error.message()); }
                                          });
            }
//            else{ spdlog::warn("{}: {}", ip_string, ec.message()); }
        });
    }
    catch(std::exception &e)
    {
//      spdlog::error("{}: {}", ip_string, e.what());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void async_send_udp_broadcast(boost::asio::io_service &io_service,
                              const std::string &str,
                              uint16_t port)
{
    async_send_udp_broadcast(io_service, std::vector<uint8_t>(str.begin(), str.end()), port);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void async_send_udp_broadcast(boost::asio::io_service &io_service,
                              const std::vector<uint8_t> &bytes,
                              uint16_t port)
{
    try
    {
        // set broadcast endpoint
        udp::endpoint receiver_endpoint(address_v4::broadcast(), port);

        udp::socket socket(io_service, udp::v4());
        socket.set_option(udp::socket::reuse_address(true));
        socket.set_option(boost::asio::socket_base::broadcast(true));

        socket.async_send_to(boost::asio::buffer(bytes), receiver_endpoint,
                             [bytes](const boost::system::error_code &error,
                                     std::size_t /*bytes_transferred*/)
                             {
                                 if(error)
                                 {
//                                     LOG_ERROR << error.message();
                                 }
                             });
    }
    catch(std::exception&)
    {
//        LOG_ERROR << e.what();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct udp_server_impl
{
public:
    udp_server_impl(boost::asio::io_service &io_service, udp_server::receive_cb_t f) :
            socket(io_service),
            recv_buffer(1 << 20),
            receive_function(std::move(f)){}

    ~udp_server_impl()
    {
        try{ socket.close(); }
        catch(std::exception&){ /*e.what();*/ }
    }

    udp::socket socket;
    udp::endpoint remote_endpoint;
    std::vector<uint8_t> recv_buffer;
    udp_server::receive_cb_t receive_function;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

udp_server::udp_server(boost::asio::io_service &io_service, receive_cb_t f) :
        m_impl(std::make_unique<udp_server_impl>(io_service, f))
{

}

udp_server::udp_server(udp_server &&the_other) noexcept
{
    std::swap(m_impl, the_other.m_impl);
}

udp_server &udp_server::operator=(udp_server the_other)
{
    std::swap(m_impl, the_other.m_impl);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void udp_server::set_receive_function(receive_cb_t f)
{
    m_impl->receive_function = std::move(f);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void udp_server::set_receive_buffer_size(size_t sz)
{
    m_impl->recv_buffer.resize(sz);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void udp_server::start_listen(uint16_t port)
{
    if(!m_impl){ return; }
    std::weak_ptr<udp_server_impl> weak_impl = m_impl;

    try
    {
        if(!m_impl->socket.is_open())
        {
            m_impl->socket.open(udp::v4());
            m_impl->socket.bind(udp::endpoint(udp::v4(), port));
        }

    }
    catch(std::exception &){ return; }

    if(port != m_impl->socket.local_endpoint().port())
    {
        m_impl->socket.connect(udp::endpoint(udp::v4(), port));
    }

    auto receive_fn = [this, weak_impl](const boost::system::error_code &error,
                                        std::size_t bytes_transferred)
    {
        if(!error)
        {
            auto impl = weak_impl.lock();

            if(impl && impl->receive_function)
            {
                try
                {
                    std::vector<uint8_t> datavec(impl->recv_buffer.begin(),
                                                 impl->recv_buffer.begin() +
                                                 bytes_transferred);
                    impl->receive_function(std::move(datavec),
                                           impl->remote_endpoint.address().to_string(),
                                           impl->remote_endpoint.port());
                }
                catch(std::exception &)
                {
//                    LOG_WARNING << e.what();
                }
            }
            if(impl && impl->socket.is_open())
            {
                start_listen(impl->socket.local_endpoint().port());
            }
        }
        else
        {
//            LOG_WARNING << error.message();
        }
    };
    m_impl->socket.async_receive_from(boost::asio::buffer(m_impl->recv_buffer), m_impl->remote_endpoint, receive_fn);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void udp_server::stop_listen()
{
    if(m_impl){ m_impl->socket.close(); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t udp_server::listening_port() const
{
    return m_impl->socket.local_endpoint().port();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct tcp_server_impl
{
    tcp::acceptor acceptor;
    tcp::socket socket;

    tcp_server::tcp_connection_callback connection_callback;

    tcp_server_impl(boost::asio::io_service &io_service,
                    tcp_server::tcp_connection_callback ccb) :
            acceptor(io_service),
            socket(io_service),
            connection_callback(std::move(ccb)){}

    ~tcp_server_impl()
    {
        acceptor.close();
    }

    void accept()
    {
        acceptor.async_accept(socket, [this](boost::system::error_code ec)
        {
            if(!ec)
            {
                tcp_connection_ptr con(new tcp_connection());
                con->m_impl = std::make_shared<tcp_connection_impl>(std::move(socket));

                // Start the persistent actor that checks for deadline expiry.
                con->check_deadline();

//                con->set_tcp_receive_cb([](tcp_connection_ptr, std::vector<uint8_t> data)
//                                        {
////                                            LOG_DEBUG << std::string(data.begin(), data.end());
//                                        });
                if(connection_callback){ connection_callback(con); }
                con->start_receive();
                accept();
            }
        });
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_server::tcp_server(boost::asio::io_service &io_service, tcp_connection_callback ccb) :
        m_impl(std::make_unique<tcp_server_impl>(io_service, std::move(ccb))){}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_server::tcp_server() = default;

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_server::~tcp_server() = default;

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_server::tcp_server(tcp_server &&other) noexcept
{
    std::swap(m_impl, other.m_impl);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_server &tcp_server::operator=(tcp_server other)
{
    std::swap(m_impl, other.m_impl);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_server::set_connection_callback(tcp_connection_callback ccb)
{
    m_impl->connection_callback = std::move(ccb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool tcp_server::start_listen(uint16_t port)
{
    if(!m_impl){ return false; }

    if(!m_impl->acceptor.is_open() ||
       port != m_impl->acceptor.local_endpoint().port())
    {
        try
        {
            m_impl->acceptor.close();
            m_impl->acceptor.open(tcp::v4());
            boost::asio::socket_base::reuse_address option(true);
            m_impl->acceptor.set_option(option);
            m_impl->acceptor.bind(tcp::endpoint(tcp::v4(), port));
            m_impl->acceptor.listen();
        }
        catch(std::exception &){ return false; }
    }
    m_impl->accept();
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t tcp_server::listening_port() const
{
    return m_impl->acceptor.local_endpoint().port();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_server::stop_listen()
{
    m_impl->acceptor.close();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct tcp_connection_impl
{
    explicit tcp_connection_impl(tcp::socket s, tcp_connection::tcp_receive_cb_t f = {}) :
            socket(std::move(s)),
            recv_buffer(8192),
#if BOOST_VERSION < 107000
            m_deadline_timer(socket.get_io_service()),
#else
            m_deadline_timer(socket.get_executor()),
#endif
            m_timeout(duration_t(0.0)),
            tcp_receive_cb(std::move(f))
    {
        m_deadline_timer.expires_at(steady_clock::time_point::max());
    }

    ~tcp_connection_impl()
    {
        try
        {
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            socket.close();
        }
        catch(std::exception &){}
    }

    tcp::socket socket;
    std::vector<uint8_t> recv_buffer;
    boost::asio::basic_waitable_timer<steady_clock> m_deadline_timer;
    duration_t m_timeout;

    // additional receive callback with connection context
    tcp_connection::tcp_receive_cb_t tcp_receive_cb;

    // used by Connection interface
    Connection::connection_cb_t m_connect_cb, m_disconnect_cb;
    Connection::receive_cb_t m_receive_cb;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_connection_ptr tcp_connection::create(boost::asio::io_service &io_service,
                                          const std::string &ip,
                                          uint16_t port,
                                          tcp_receive_cb_t f)
{
    auto ret = tcp_connection_ptr(new tcp_connection(io_service, std::move(f)));

    auto resolver_ptr = std::make_shared<tcp::resolver>(io_service);

    resolver_ptr->async_resolve({ip, std::to_string(port)}, [ret, resolver_ptr, ip]
            (const boost::system::error_code &ec,
             tcp::resolver::iterator end_point_it)
    {
        if(!ec)
        {
            try
            {
                boost::asio::async_connect(ret->m_impl->socket, std::move(end_point_it),
                                           [ret](const boost::system::error_code &ec,
                                                 const tcp::resolver::iterator &/*end_point_it*/)
                                           {
                                               if(!ec)
                                               {
                                                   if(ret->m_impl->m_connect_cb)
                                                   {
                                                       ret->m_impl->m_connect_cb(ret);
                                                   }
                                                   ret->start_receive();
                                               }
                                           });
            }
            catch(std::exception &)
            {
//                LOG_WARNING << ip << ": " << e.what();
            }
        }
//        else{ LOG_WARNING << ip << ": " << ec.message(); }
    });
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_connection::tcp_connection(boost::asio::io_service &io_service,
                               tcp_receive_cb_t f) :
        m_impl(new tcp_connection_impl(tcp::socket(io_service), std::move(f)))
{
    // Start the persistent actor that checks for deadline expiry.
    check_deadline();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

tcp_connection::~tcp_connection()
{
    try{ m_impl->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive); }
    catch(std::exception &){}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool tcp_connection::open()
{
    return m_impl && m_impl->socket.is_open();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t tcp_connection::write_bytes(const void *data, size_t num_bytes)
{
    auto bytes = std::vector<uint8_t>((uint8_t *) data, (uint8_t *) data + num_bytes);
    auto buffer = boost::asio::buffer(bytes.data(), bytes.size());
    auto impl_cp = m_impl;

    if(impl_cp->m_timeout != duration_t(0))
    {
        auto dur = duration_cast<steady_clock::duration>(impl_cp->m_timeout);
        impl_cp->m_deadline_timer.expires_from_now(dur);
    }

    boost::asio::async_write(m_impl->socket, buffer, [impl_cp, bytes{std::move(bytes)}]
            (const boost::system::error_code &error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            if(bytes_transferred < bytes.size())
            {
//                LOG_WARNING << "not all bytes written";
            }
        }
        else
        {
            switch(error.value())
            {
                case boost::asio::error::bad_descriptor:
//                default:LOG_TRACE_2 << error.message() << " (" << error.value() << ")";
                    break;
            }
        }
    });
    return num_bytes;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t tcp_connection::read_bytes(void */*buffer*/, size_t /*num_bytes*/)
{
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::set_tcp_receive_cb(tcp_receive_cb_t tcp_cb)
{
    m_impl->tcp_receive_cb = std::move(tcp_cb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::start_receive()
{
    auto impl_cp = m_impl;
    auto weak_self = std::weak_ptr<tcp_connection>(shared_from_this());

    if(impl_cp->m_timeout != duration_t(0))
    {
        auto dur = duration_cast<steady_clock::duration>(impl_cp->m_timeout);
        impl_cp->m_deadline_timer.expires_from_now(dur);
    }

    impl_cp->socket.async_receive(boost::asio::buffer(impl_cp->recv_buffer), [impl_cp, weak_self]
            (const boost::system::error_code &error, std::size_t bytes_transferred)
    {
        auto self = weak_self.lock();

        if(!error)
        {
            if(bytes_transferred)
            {
                if(self && impl_cp->tcp_receive_cb)
                {
                    std::vector<uint8_t> datavec(impl_cp->recv_buffer.begin(),
                                                 impl_cp->recv_buffer.begin() +
                                                 bytes_transferred);
                    impl_cp->tcp_receive_cb(self, std::move(datavec));
                }
                if(self && impl_cp->m_receive_cb)
                {
                    std::vector<uint8_t> datavec(impl_cp->recv_buffer.begin(),
                                                 impl_cp->recv_buffer.begin() +
                                                 bytes_transferred);
                    impl_cp->m_receive_cb(self, std::move(datavec));
                }
//                LOG_TRACE_2 << "tcp: received " << bytes_transferred << " bytes";
            }

            // only keep receiving if there are any refs on this instance left
            if(self){ self->start_receive(); }
        }
        else
        {
            switch(error.value())
            {
                case boost::asio::error::eof:
                case boost::asio::error::connection_reset:
                    impl_cp->socket.close();
                    [[fallthrough]];
                case boost::asio::error::operation_aborted:
                case boost::asio::error::bad_descriptor:
                {
                    std::string str = "tcp_connection";
                    if(self){ str = self->description(); }
//                    LOG_TRACE_1 << "disconnected: " << str;
                }

                    if(self && impl_cp->m_disconnect_cb)
                    {
                        auto disconnect_cb = std::move(impl_cp->m_disconnect_cb);
                        disconnect_cb(self);
                    }

//                default:LOG_TRACE_2 << error.message() << " (" << error.value() << ")";
                    break;
            }
        }
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::check_deadline()
{
    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if(m_impl->m_deadline_timer.expires_at() <= steady_clock::now())
    {
//        LOG_TRACE_2 << "connection timeout (" << to_string(m_impl->m_timeout.count(), 2) << ")";

        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled. This allows the blocked
        // connect(), read_line() or write_line() functions to return.
        boost::system::error_code ignored_ec;
        m_impl->socket.close(ignored_ec);

        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        m_impl->m_deadline_timer.expires_at(steady_clock::time_point::max());
    }

    std::weak_ptr<tcp_connection_impl> weak_impl = m_impl;

    // Put the actor back to sleep.
    m_impl->m_deadline_timer.async_wait([this, weak_impl]
                                                (const boost::system::error_code &/*ec*/)
                                        {
                                            auto impl = weak_impl.lock();
                                            if(impl){ check_deadline(); }
                                        });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::close()
{
    try
    {
        m_impl->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        m_impl->socket.close();
    }
    catch(std::exception &){}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool tcp_connection::is_open() const
{
    return m_impl->socket.is_open();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t tcp_connection::port() const
{
    try{ return m_impl->socket.local_endpoint().port(); }
    catch(std::exception &){}
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::string tcp_connection::remote_ip() const
{
    try{ return m_impl->socket.remote_endpoint().address().to_string(); }
    catch(std::exception &){}
    return UNKNOWN_IP;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t tcp_connection::remote_port() const
{
    try{ return m_impl->socket.remote_endpoint().port(); }
    catch(std::exception &){}
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t tcp_connection::available() const
{
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::drain()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::string tcp_connection::description() const
{
    return "tcp_connection: " + remote_ip() + " (" + std::to_string(remote_port()) + ")";
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::set_receive_cb(receive_cb_t cb)
{
    m_impl->m_receive_cb = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::set_connect_cb(connection_cb_t cb)
{
    m_impl->m_connect_cb = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::set_disconnect_cb(connection_cb_t cb)
{
    m_impl->m_disconnect_cb = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

double tcp_connection::timeout() const
{
    return m_impl->m_timeout.count();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void tcp_connection::set_timeout(double timeout_secs)
{
    m_impl->m_timeout = duration_t(timeout_secs);
    m_impl->m_deadline_timer.expires_from_now(duration_cast<steady_clock::duration>(m_impl->m_timeout));
}

}// namespaces
