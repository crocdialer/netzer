#include <curl/curl.h>
#include <mutex>
#include <cstring>
#include <map>
#include "netzer/http.hpp"

using duration_t = std::chrono::duration<double>;

namespace netzer::http
{

using ActionPtr = std::shared_ptr<class CurlAction>;
using handle_map_t = std::map<CURL *, ActionPtr>;

///////////////////////////////////////////////////////////////////////////////

struct ClientImpl
{
    std::shared_ptr<CURLM> m_curl_multi_handle;

    // map containing current handles
    handle_map_t m_handle_map;

    // mutex to protect the handle-map
    std::mutex m_mutex;

    // connection timeout in ms
    uint64_t m_timeout;

    // number of running transfers
    int m_num_connections;

    explicit ClientImpl() :
            m_curl_multi_handle(curl_multi_init(), curl_multi_cleanup),
            m_timeout(Client::DEFAULT_TIMEOUT),
            m_num_connections(0) {};

    void poll();

    void add_action(const ActionPtr &action, completion_cb_t ch, progress_cb_t ph = progress_cb_t());
};

///////////////////////////////////////////////////////////////////////////////

class CurlAction
{
private:
    std::unique_ptr<CURL, std::function<void(CURL*)>> m_curl_handle;
    std::chrono::steady_clock::time_point m_start_time;
    completion_cb_t m_completion_handler;
    progress_cb_t m_progress_handler;
    response_t m_response;

    ///////////////////////////////////////////////////////////////////////////////

    /*!
     * callback to process incoming data
     */
    static size_t write_static(void *buffer, size_t num_elems, size_t num_elem_bytes, void *userp)
    {
        size_t num_bytes = num_elems * num_elem_bytes;

        if(userp)
        {
            auto *ourAction = static_cast<CurlAction *>(userp);
            auto *buf_start = (uint8_t *)(buffer);
            uint8_t *buf_end = buf_start + num_bytes;
            ourAction->m_response.data.insert(ourAction->m_response.data.end(), buf_start, buf_end);
        }
        return num_bytes;
    }

    ///////////////////////////////////////////////////////////////////////////////

    /*!
     * callback for data provided to Curl for sending
     */
    static size_t read_static(void *ptr, size_t num_elems, size_t num_elem_bytes, void *in)
    {
        size_t max_num_bytes = num_elems * num_elem_bytes;
        size_t num_bytes = max_num_bytes;
        memcpy(ptr, in, num_bytes);
        return num_bytes;
    }

    ///////////////////////////////////////////////////////////////////////////////

    /*!
     * callback to monitor transfer progress
     */
    static int progress_static(void *userp, double dltotal, double dlnow, double ult, double uln)
    {
        auto *self = static_cast<CurlAction *>(userp);
        auto &con = self->m_response.connection;
        con.dl_total = dltotal;
        con.dl_now = dlnow;
        con.ul_total = ult;
        con.ul_now = uln;
        if(self->m_progress_handler){ self->m_progress_handler(con); }
        return 0;
    }

public:
    explicit CurlAction(const std::string &the_url) :
            m_curl_handle(curl_easy_init(), curl_easy_cleanup),
            m_start_time(std::chrono::steady_clock::now())
    {
        m_response.connection = {the_url, 0, 0, 0, 0, 0};
        curl_easy_setopt(handle(), CURLOPT_WRITEDATA, this);
        curl_easy_setopt(handle(), CURLOPT_WRITEFUNCTION, write_static);
        curl_easy_setopt(handle(), CURLOPT_READDATA, this);
        curl_easy_setopt(handle(), CURLOPT_READFUNCTION, read_static);
        curl_easy_setopt(handle(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handle(), CURLOPT_PROGRESSDATA, this);
        curl_easy_setopt(handle(), CURLOPT_PROGRESSFUNCTION, progress_static);
        curl_easy_setopt(handle(), CURLOPT_URL, the_url.c_str());
    };

    ///////////////////////////////////////////////////////////////////////////////

    response_t &response() { return m_response; };

    ///////////////////////////////////////////////////////////////////////////////

    bool perform()
    {
        CURLcode curlResult = curl_easy_perform(handle());
        curl_easy_getinfo(handle(), CURLINFO_RESPONSE_CODE, &m_response.status_code);
        m_response.duration = duration();
        if(!curlResult && m_completion_handler) m_completion_handler(m_response);
        return !curlResult;
    }

    ///////////////////////////////////////////////////////////////////////////////

    [[nodiscard]] CURL *handle() const { return m_curl_handle.get(); }

    ///////////////////////////////////////////////////////////////////////////////

    [[nodiscard]] connection_info_t connection_info() const { return m_response.connection; }

    ///////////////////////////////////////////////////////////////////////////////

    [[nodiscard]] completion_cb_t completion_handler() const { return m_completion_handler; }

    void set_completion_handler(completion_cb_t ch) { m_completion_handler = std::move(ch); }

    void set_progress_handler(progress_cb_t ph) { m_progress_handler = std::move(ph); }

    ///////////////////////////////////////////////////////////////////////////////

    void set_timeout(uint64_t timeout)
    {
        m_response.connection.timeout = timeout;
        curl_easy_setopt(handle(), CURLOPT_TIMEOUT, timeout);
    }

    ///////////////////////////////////////////////////////////////////////////////

    [[nodiscard]] double duration() const
    {
        return duration_t(std::chrono::steady_clock::now() - m_start_time).count();
    }
};

///////////////////////////////////////////////////////////////////////////////

using Action_GET = CurlAction;

///////////////////////////////////////////////////////////////////////////////

class Action_POST : public CurlAction
{
private:
    std::vector<uint8_t> m_data;
    std::shared_ptr<struct curl_slist> m_headers;

public:
    Action_POST(const std::string &the_url,
                std::vector<uint8_t> the_data,
                const std::string &the_mime_type) :
            CurlAction(the_url),
            m_data(std::move(the_data))
    {
        auto header_content = "Content-Type: " + the_mime_type;
        m_headers = std::shared_ptr<struct curl_slist>(curl_slist_append(nullptr,
                                                                         header_content.c_str()),
                                                       curl_slist_free_all);

        curl_easy_setopt(handle(), CURLOPT_URL, the_url.c_str());
        curl_easy_setopt(handle(), CURLOPT_POSTFIELDS, &m_data[0]);
        curl_easy_setopt(handle(), CURLOPT_POSTFIELDSIZE, static_cast<curl_off_t>(m_data.size()));
        curl_easy_setopt(handle(), CURLOPT_HTTPHEADER, m_headers.get());
    }
};

///////////////////////////////////////////////////////////////////////////////

class Action_PUT : public CurlAction
{
private:
    std::vector<uint8_t> m_data;
    std::shared_ptr<struct curl_slist> m_headers;

public:
    Action_PUT(const std::string &the_url,
               std::vector<uint8_t> the_data,
               const std::string &the_mime_type) :
            CurlAction(the_url),
            m_data(std::move(the_data))
    {
        auto header_content = "Content-Type: " + the_mime_type;
        m_headers = std::shared_ptr<struct curl_slist>(curl_slist_append(nullptr,
                                                                         header_content.c_str()),
                                                       curl_slist_free_all);

        curl_easy_setopt(handle(), CURLOPT_URL, the_url.c_str());

        /* enable uploading */
        curl_easy_setopt(handle(), CURLOPT_UPLOAD, 1L);

        /* HTTP PUT please */
        curl_easy_setopt(handle(), CURLOPT_PUT, 1L);
        curl_easy_setopt(handle(), CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(m_data.size()));
        curl_easy_setopt(handle(), CURLOPT_HTTPHEADER, m_headers.get());
    }
};

///////////////////////////////////////////////////////////////////////////////

class Action_DELETE : public CurlAction
{
public:
    explicit Action_DELETE(const std::string &the_url) :
            CurlAction(the_url)
    {
        curl_easy_setopt(handle(), CURLOPT_CUSTOMREQUEST, "DELETE");
    }
};

///////////////////////////////////////////////////////////////////////////////

response_t head(const std::string &url)
{
    ActionPtr url_action = std::make_unique<Action_GET>(url);
    curl_easy_setopt(url_action->handle(), CURLOPT_NOBODY, 1L);
    url_action->perform();
    return url_action->response();
}

///////////////////////////////////////////////////////////////////////////////

response_t get(const std::string &url)
{
    ActionPtr url_action = std::make_unique<Action_GET>(url);
    url_action->perform();
    return url_action->response();
}

///////////////////////////////////////////////////////////////////////////////

response_t post(const std::string &url,
                const std::vector<uint8_t> &data,
                const std::string &mime_type)
{
    ActionPtr url_action = std::make_shared<Action_POST>(url, data, mime_type);
    url_action->perform();
    return url_action->response();
}

///////////////////////////////////////////////////////////////////////////////

response_t put(const std::string &url,
               const std::vector<uint8_t> &data,
               const std::string &mime_type)
{
    ActionPtr url_action = std::make_shared<Action_PUT>(url, data, mime_type);
    url_action->perform();
    return url_action->response();
}

///////////////////////////////////////////////////////////////////////////////

response_t del(const std::string &url)
{
    ActionPtr url_action = std::make_shared<Action_DELETE>(url);
    url_action->perform();
    return url_action->response();
}

///////////////////////////////////////////////////////////////////////////////

Client::Client() :
        m_impl(std::make_unique<ClientImpl>())
{

}

///////////////////////////////////////////////////////////////////////////////

Client &Client::operator=(Client other)
{
    std::swap(m_impl, other.m_impl);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

void ClientImpl::poll()
{
    curl_multi_perform(m_curl_multi_handle.get(), &m_num_connections);
    int msgs_left;
    CURLMsg *msg = curl_multi_info_read(m_curl_multi_handle.get(), &msgs_left);

    while(msg)
    {
        if(msg->msg == CURLMSG_DONE)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            CURL *easy = msg->easy_handle;
            CURLcode res = msg->data.result;
            curl_multi_remove_handle(m_curl_multi_handle.get(), easy);
            auto itr = m_handle_map.find(easy);

            if(itr != m_handle_map.end())
            {
                auto &response = itr->second->response();
                response.duration = itr->second->duration();

                if(!res)
                {
                    // http response code
                    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &itr->second->response().status_code);
                    auto ci = itr->second->connection_info();
                    if(itr->second->completion_handler()){ itr->second->completion_handler()(response); }
                }else
                {
                    // TODO: error callback!?
                }
                m_handle_map.erase(itr);
            }
        }
        msg = curl_multi_info_read(m_curl_multi_handle.get(), &msgs_left);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ClientImpl::add_action(const ActionPtr &action, completion_cb_t ch, progress_cb_t ph)
{
    // set options for this handle
    action->set_timeout(m_timeout);
    action->set_completion_handler(std::move(ch));
    action->set_progress_handler(std::move(ph));

    std::unique_lock<std::mutex> lock(m_mutex);
    m_handle_map[action->handle()] = action;

    // add handle to multi
    curl_multi_add_handle(m_curl_multi_handle.get(), action->handle());

//    if(m_io_service && !m_num_connections)
//    {
//        m_io_service->post([this] { poll(); });
//    }
}

///////////////////////////////////////////////////////////////////////////////

void Client::async_head(const std::string &url,
                        completion_cb_t ch,
                        progress_cb_t ph)
{
    ActionPtr url_action = std::make_shared<Action_GET>(url);
    curl_easy_setopt(url_action->handle(), CURLOPT_NOBODY, 1L);
    m_impl->add_action(url_action, std::move(ch), std::move(ph));
}

///////////////////////////////////////////////////////////////////////////////

void Client::async_get(const std::string &url,
                       completion_cb_t completion_cb,
                       progress_cb_t ph)
{
    // create an action which holds an easy handle
    ActionPtr url_action = std::make_shared<Action_GET>(url);
    m_impl->add_action(url_action, std::move(completion_cb), std::move(ph));
}

///////////////////////////////////////////////////////////////////////////////

void Client::async_post(const std::string &url,
                        const std::vector<uint8_t> &data,
                        completion_cb_t completion_cb,
                        const std::string &mime_type,
                        progress_cb_t progress_cb)
{
    ActionPtr url_action = std::make_shared<Action_POST>(url, data, mime_type);
    m_impl->add_action(url_action, std::move(completion_cb), std::move(progress_cb));
}

///////////////////////////////////////////////////////////////////////////////

void Client::async_put(const std::string &url,
                       const std::vector<uint8_t> &data,
                       completion_cb_t completion_cb,
                       const std::string &mime_type,
                       progress_cb_t progress_cb)
{
    ActionPtr url_action = std::make_shared<Action_PUT>(url, data, mime_type);
    m_impl->add_action(url_action, std::move(completion_cb), std::move(progress_cb));
}

///////////////////////////////////////////////////////////////////////////////

void Client::async_del(const std::string &url, completion_cb_t completion_cb)
{
    ActionPtr url_action = std::make_shared<Action_DELETE>(url);
    m_impl->add_action(url_action, std::move(completion_cb));
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Client::timeout() const
{
    return m_impl->m_timeout;
}

///////////////////////////////////////////////////////////////////////////////

void Client::set_timeout(uint64_t t)
{
    m_impl->m_timeout = t;
}

///////////////////////////////////////////////////////////////////////////////

void Client::poll() { m_impl->poll(); }

///////////////////////////////////////////////////////////////////////////////

}// namespace
