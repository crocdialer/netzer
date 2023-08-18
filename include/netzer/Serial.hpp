
#pragma once

#include <map>
#include <set>
#include "Connection.hpp"

// forward declare boost io_service
namespace boost::asio{ class io_context; }
namespace crocore{ using io_service_t = boost::asio::io_context; }

namespace crocore::net
{

NETZER_DEFINE_CLASS_PTR(Serial);

class Serial : public crocore::net::Connection, public std::enable_shared_from_this<Serial>
{

public:

    static SerialPtr create(io_service_t &io, receive_cb_t cb = {});

    virtual ~Serial();

    static std::vector<std::string>
    device_list(const std::set<std::string> &the_patterns = std::set<std::string>());

    static std::map<std::string, SerialPtr> connected_devices();

    bool open(const std::string &the_name, int the_baudrate = 57600);

    bool open() override;

    void close() override;

    bool is_open() const override;

    size_t read_bytes(void *buffer, size_t sz) override;

    size_t write_bytes(const void *buffer, size_t sz) override;

    size_t available() const override;

    std::string description() const override;

    void drain() override;

    void set_receive_cb(receive_cb_t the_cb) override;

    void set_connect_cb(connection_cb_t cb) override;

    void set_disconnect_cb(connection_cb_t cb) override;

private:

    void async_read_bytes();

    void async_write_bytes(const void *buffer, size_t sz);

    Serial(io_service_t &io, receive_cb_t cb);

    std::shared_ptr<struct SerialImpl> m_impl;
};

//----------------------------------------------------------------------
}
