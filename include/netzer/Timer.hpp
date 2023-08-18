

#pragma once

#include <chrono>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>

// forward declare boost io_service
namespace boost::asio{ class io_context; }
namespace crocore{ using io_service_t = boost::asio::io_context; }

namespace crocore
{

class Stopwatch
{
public:

    Stopwatch();

    /*!
     * start the timer. has no effect, if the timer is already running.
     */
    void start();

    /*!
     * stop the timer. all measured laps are kept.
     */
    void stop();

    /*!
     * return true if the timer is currently running.
     */
    [[nodiscard]] bool running() const;

    /*!
     * return the total time (in seconds) measured, including all previous laps.
     */
    [[nodiscard]] double time_elapsed() const;

    /*!
     * return the time (in seconds) measured for the current lap.
     */
    [[nodiscard]] double time_elapsed_for_lap() const;

    /*!
     * reset the timer. this stops time measurement, if it was running before, and clears all
     * measured laps.
     */
    void reset();

    /*!
     * begin measurement of a new lap. if the timer is not running, this call has no effect.
     */
    void new_lap();

    /*!
     * return the values for all previously measured laps.
     */
    [[nodiscard]] const std::vector<double> &laps() const;

private:
    bool m_running = false;
    std::chrono::steady_clock::time_point m_start_time;
    std::vector<double> m_laps;
};

class Timer
{
public:
    typedef std::function<void(void)> timer_cb_t;

    Timer() = default;

    Timer(const Timer&) = delete;

    Timer(Timer &&other) noexcept;

    Timer& operator=(Timer other);

    friend void swap(Timer &lhs, Timer &rhs);

    explicit Timer(io_service_t &io, timer_cb_t cb = timer_cb_t());

    /*!
     * set expiration date from now in seconds
     */
    void expires_from_now(double secs);

    /*!
     * get expiration date from now in seconds
     */
    [[nodiscard]] double expires_from_now() const;

    /*!
     * returns true if the timer has expired
     */
    [[nodiscard]] bool has_expired() const;

    /*!
     * cancel a currently running timer
     */
    void cancel();

    /*!
     * returns true if the timer is set to fire periodically
     */
    [[nodiscard]] bool periodic() const;

    /*!
     * sets if the timer should fire periodically
     */
    void set_periodic(bool b = true);

    /*!
     * set the function object to call when the timer expires
     */
    void set_callback(timer_cb_t cb = timer_cb_t());

private:
    std::shared_ptr<struct timer_impl> m_impl;
};
}
