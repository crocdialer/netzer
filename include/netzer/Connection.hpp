// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

//  Connection.hpp
//  kinskiGL
//
//  Created by Fabian on 05/04/16.

#pragma once

#include "define_class_ptr.hpp"

namespace netzer
{

NETZER_DEFINE_CLASS_PTR(Connection)

//! Connection interface
class Connection
{
public:

    using connection_cb_t = std::function<void(ConnectionPtr)>;
    using receive_cb_t = std::function<void(ConnectionPtr, const std::vector<uint8_t> &)>;

    //! open the device
    virtual bool open() = 0;

    //! close the device, cancel current transfers
    virtual void close() = 0;

    //! returns true if the device is initialized and ready to transfer
    [[nodiscard]] virtual bool is_open() const = 0;

    //! reads up to sz bytes into buffer, returns the number of bytes actually read.
    // most important: this call makes only sense when no receive_cb is provided.
    // otherwise you won't see any bytes here.
    virtual size_t read_bytes(void *buffer, size_t sz) = 0;

    //! transfer sz bytes from buffer, returns the number of bytes actually written
    virtual size_t write_bytes(const void *buffer, size_t sz) = 0;

    //! returns the number of bytes available for reading
    [[nodiscard]] virtual size_t available() const = 0;

    //! empty buffers, cancel current transfers
    virtual void drain() = 0;

    //! returns a textual description for this device
    [[nodiscard]] virtual std::string description() const = 0;

    //! set a receive callback, that triggers when data is available for reading
    virtual void set_receive_cb(receive_cb_t the_cb) = 0;

    //! set a connect callback, that fires when the connection is succesfully established
    virtual void set_connect_cb(connection_cb_t cb) = 0;

    //! set a disconnect callback, that fires when the connection is closed
    virtual void set_disconnect_cb(connection_cb_t cb) = 0;

    //! c-strings
    inline size_t write(const char *cstring)
    {
        return write_bytes(cstring, strlen(cstring));
    }

    //! std-strings
    inline size_t write(const std::string &string)
    {
        return write_bytes(string.data(), string.size());
    }

    //! template to transfer the content of generic containers
    template<typename T>
    inline size_t write(const T &container)
    {
        return write(std::vector<uint8_t>(std::begin(container), std::end(container)));
    }

    //! template to transfer the content of std::vector
    template<typename T>
    inline size_t write(const std::vector<T> &array)
    {
        return write_bytes(array.data(), array.size() * sizeof(T));
    }
};

}// namespace netzer
