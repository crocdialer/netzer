//
//  http.h
//  Kinski Framework
//
//  Created by Fabian Schmidt on 11/20/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace crocore::net::http
{
    
struct connection_info_t
{
    std::string url;
    double dl_total = 0, dl_now = 0, ul_total = 0, ul_now = 0;
    uint64_t timeout = 0;
};
    
struct response_t
{
    connection_info_t connection;
    uint64_t status_code = 0;
    std::vector<uint8_t> data;
    double duration = 0.0;
};
    
using progress_cb_t = std::function<void(connection_info_t)>;
using completion_cb_t = std::function<void(response_t&)>;

/*!
 * get the resource at the given url (blocking) with HTTP HEAD
 */
response_t head(const std::string &url);
    
/*!
 * get the resource at the given url (blocking) with HTTP GET
 */
response_t get(const std::string &url);

/*!
 * get the resource at the given url (blocking) with HTTP POST.
 * transmits <data> with the provided MIME-type.
 */
response_t post(const std::string &url,
                const std::vector<uint8_t> &data,
                const std::string &mime_type = "application/json");
    
/*!
 * upload <data> at the given url (blocking) with HTTP PUT.
 * transmits <data> and sets the Content-Type attribute to <mime_type>.
 */
response_t put(const std::string &url,
               const std::vector<uint8_t> &data,
               const std::string &mime_type = "application/json");

/*!
 * http DELETE
 */
response_t del(const std::string &url);

class Client
{
public:

    // Timeout interval for http requests
    static constexpr uint64_t DEFAULT_TIMEOUT = 0;

    explicit Client();

    Client(const Client &other) = delete;
    Client(Client &&other) noexcept = default;

    Client& operator=(Client other);

    /*!
     * get the resource at the given url (non-blocking) with HTTP HEAD
     */
    void async_head(const std::string &url,
                    completion_cb_t completion_cb = {},
                    progress_cb_t progress_cb = {});
    
    /*!
     * get the resource at the given url (non-blocking) with HTTP GET
     */
    void async_get(const std::string &url,
                   completion_cb_t completion_cb = {},
                   progress_cb_t progress_cb = {});
    
    /*!
     * get the resource at the given url (non-blocking) with HTTP POST
     */
    void async_post(const std::string &url,
                    const std::vector<uint8_t> &data,
                    completion_cb_t completion_cb = {},
                    const std::string &mime_type = "application/json",
                    progress_cb_t progress_cb = {});
    
    /*!
     * upload <data> at the given url (non-blocking) with HTTP PUT.
     * transmits <data> and sets the Content-Type attribute to <mime_type>.
     */
    void async_put(const std::string &url,
                   const std::vector<uint8_t> &data,
                   completion_cb_t completion_cb = {},
                   const std::string &mime_type = "application/json",
                   progress_cb_t progress_cb = {});
    /*!
     * send an http DELETE request (non-blocking)
     */
    void async_del(const std::string &url,
                   completion_cb_t completion_cb = {});
    
    /*!
     * return the currently applied timeout for connections
     */
    [[nodiscard]] uint64_t timeout() const;
    
    /*!
     * set the timeout for connections
     */
    void set_timeout(uint64_t t);

    /*!
     * manually poll
     */
    void poll();

private:
    std::unique_ptr<struct ClientImpl> m_impl;
};
    
}// namespace
