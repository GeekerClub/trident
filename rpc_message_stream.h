// Copyright (c) 2014 Baidu.com, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: qinzuoyan01@baidu.com (Qin Zuoyan)

#ifndef _SOFA_PBRPC_RPC_MESSAGE_STREAM_H_
#define _SOFA_PBRPC_RPC_MESSAGE_STREAM_H_

#include <deque>

#include <trident/rpc_byte_stream.h>
#include <trident/buffer.h>
#include <trident/rpc_error_code.h>
#include <trident/rpc_message_header.h>
#include <trident/tran_buf_pool.h>
#include <trident/flow_controller.h>

namespace trident {
namespace pbrpc {

// The "SendCookie" type should has default constructor, and
// should be copyable. 
template <typename SendCookie>
class RpcMessageStream : public RpcByteStream
{
public:
    enum RoleType {
        ROLE_TYPE_SERVER = 0,
        ROLE_TYPE_CLIENT = 1,
    };

public:
    RpcMessageStream(RoleType role_type, IOService& io_service, const RpcEndpoint& endpoint)
        : RpcByteStream(io_service, endpoint)
        , _role_type(role_type)
        , _pending_message_count(0)
        , _pending_data_size(0)
        , _pending_buffer_size(0)
        , _swapped_message_count(0)
        , _swapped_data_size(0)
        , _swapped_buffer_size(0)
        , _max_pending_buffer_size(0)
        , _read_quota_token(1)
        , _write_quota_token(1)
        , _total_sent_count(0)
        , _total_sent_size(0)
        , _total_received_count(0)
        , _total_received_size(0)
        , _sent_size(0)
        , _sending_data(NULL)
        , _sending_size(0)
        , _received_message_size(0)
        , _received_header_size(0)
        , _receiving_header_identified(false)
        , _tran_buf(NULL)
        , _receiving_data(NULL)
        , _receiving_size(0)
        , _send_token(TOKEN_FREE)
        , _receive_token(TOKEN_FREE)
    {}

    virtual ~RpcMessageStream()
    {
        if (_tran_buf != NULL)
        {
            TranBufPool::free(_tran_buf);
            _tran_buf = NULL;
        }
    }

    // Async send message to the remote endpoint. The message may be first
    // put into a pending queue if the channel is busy at that time.
    //
    // Before starting sending data, the hook function "on_sending" will
    // be called to check if the message still need to be sent.
    //
    // If send succeed, callback "on_sent" will be called;
    // else, callback "on_send_failed" will be called. 
    //
    // @param message  the message to send, including header, meta and data.
    // @param cookie  a cookie affiliated to the message, which can carry some
    //                user's customized info. It will not be sent, but would
    //                be passed to the callback function.
    void async_send_message(
            const ReadBufferPtr& message,
            const SendCookie& cookie)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (is_closed())
        {
            on_send_failed(RPC_ERROR_CONNECTION_CLOSED, message, cookie);
            return;
        }

        put_into_pending_queue(message, cookie);

        try_start_send();
    }

    // Set the flow controller.
    void set_flow_controller(const FlowControllerPtr& flow_controller)
    {
        _flow_controller = flow_controller;
    }

    // Set the max size of pending buffer for send.
    void set_max_pending_buffer_size(int64 max_pending_buffer_size)
    {
        _max_pending_buffer_size = max_pending_buffer_size;
    }

    // Get the max size of pending buffer for send.
    int64 max_pending_buffer_size() const
    {
        return _max_pending_buffer_size;
    }

    // Get the current count of messages in the pending queue.
    int pending_message_count() const
    {
        return _pending_message_count + _swapped_message_count;
    }

    // Get the current data size of messages in the pending queue.
    int64 pending_data_size() const
    {
        return _pending_data_size + _swapped_data_size;
    }

    // Get the current buffer size occupied by the pending queue.
    // This size may be larger than "pending_data_size".
    int64 pending_buffer_size() const
    {
        return _pending_buffer_size + _swapped_buffer_size;
    }

    // Trigger receiving operator.
    // @return true if suceessfully triggered
    virtual bool trigger_receive()
    {
        _read_quota_token = 1;
        return try_start_receive();
    }

    // Trigger sending operator.
    // @return true if suceessfully triggered
    virtual bool trigger_send()
    {
        _write_quota_token = 1;
        return try_start_send();
    }

    // Get read quota token used for sorting the trigger order.
    // Always no more than 0.
    int read_quota_token() const
    {
        return _read_quota_token;
    }

    // Get write quota token used for sorting the trigger order.
    // Always no more than 0.
    int write_quota_token() const
    {
        return _write_quota_token;
    }

protected:
    // Hook function on the point of starting the sending.
    //
    // If return true, go ahead with the sending;
    // else, cancel the sending.
    //
    // This hook may be useful when the message stayed in a sending waiting
    // queue for some time, but when dequed, it is already timeout.
    virtual bool on_sending(
            const ReadBufferPtr& message,
            const SendCookie& cookie) = 0;

    // Callback function when send message succeed.
    virtual void on_sent(
            const ReadBufferPtr& message,
            const SendCookie& cookie) = 0;

    // Callback function when send message failed.
    virtual void on_send_failed(
            RpcErrorCode error_code,
            const ReadBufferPtr& message,
            const SendCookie& cookie) = 0;

    // Hook function when received message.
    // @param message  the rough received message, including meta and data.
    // @param meta_size  the size of meta.
    // @param data_size  the size of data.
    virtual void on_received(
            const ReadBufferPtr& message,
            int meta_size,
            int64 data_size) = 0;

private:
    virtual bool on_connected()
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        // prepare receiving
        reset_receiving_env();
        if (!reset_tran_buf())
        {
            close("out of memory");
            return false;
        }
        return true;
    }

    // Callback of "async_read_some()".
    virtual void on_read_some(
            const boost::system::error_code& error,
            std::size_t bytes_transferred)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (error)
        {
            if (error != boost::asio::error::eof)
            {
#if defined( LOG )
                LOG(ERROR) <<  "on_read_some(): " << RpcEndpointToString(_remote_endpoint)
                           << ": read error: " << error.message();
#else
                SLOG(ERROR, "on_read_some(): %s: read error: %s",
                        RpcEndpointToString(_remote_endpoint).c_str(),
                        error.message().c_str());
#endif
            }

            close(error.message());
            return;
        }

        _last_rw_ticks = _ticks;
        ++_total_received_count;
        _total_received_size += bytes_transferred;

        std::deque<ReceivedItem> received_messages;
        if (!split_and_process_message(_receiving_data, 
                    static_cast<int>(bytes_transferred), &received_messages))
        {
            close("broken stream");
            return;
        }

        _receiving_data += bytes_transferred;
        _receiving_size -= bytes_transferred;

        // check if current tran buf is used out
        if (_receiving_size == 0 && !reset_tran_buf())
        {
            close("out of memory");
            return;
        }

        // release token
        atomic_comp_swap(&_receive_token, TOKEN_FREE, TOKEN_LOCK);

        // trigger next receive
        try_start_receive();

        // process messages
        while (!is_closed() && !received_messages.empty())
        {
            const ReceivedItem& item = received_messages.front();
            on_received(item.message, item.meta_size, item.data_size);
            received_messages.pop_front();
        }
    }

    // Callback of "async_write_some()".
    virtual void on_write_some(
            const boost::system::error_code& error,
            std::size_t bytes_transferred)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (error)
        {
#if defined( LOG )
            LOG(ERROR) << "on_write_some(): " << RpcEndpointToString(_remote_endpoint)
                       << ": write error: " << error.message();
#else
            SLOG(ERROR, "on_write_some(): %s: write error: %s",
                    RpcEndpointToString(_remote_endpoint).c_str(),
                    error.message().c_str());
#endif

            on_send_failed(RPC_ERROR_CHANNEL_BROKEN, _sending_message, _sending_cookie);
            clear_sending_env();

            close(error.message());
            return;
        }

        _last_rw_ticks = _ticks;
        _sent_size += bytes_transferred;
        ++_total_sent_count;
        _total_sent_size += bytes_transferred;

        if (static_cast<int>(bytes_transferred) == _sending_size)
        {
            // seek to the next buf handle
            if (_sending_message->Next(
                        reinterpret_cast<const void**>(&_sending_data),
                        &_sending_size))
            {
                // more buf to send
                async_write_some(_sending_data, _sending_size);
            }
            else
            {
                // current message is completely sent
                SCHECK_EQ(_sent_size, _sending_message->ByteCount());

                // call hook function
                on_sent(_sending_message, _sending_cookie);
                clear_sending_env();

                // release token
                atomic_comp_swap(&_send_token, TOKEN_FREE, TOKEN_LOCK);

                // trigger next send
                try_start_send();

                // trigger next receive if it is server, because receiving operator
                // in server may be hanged for pending buffer full.
                if (_role_type == ROLE_TYPE_SERVER)
                    try_start_receive();
            }
        }
        else
        {
            // only sent part of the data
            SCHECK_LT(static_cast<int>(bytes_transferred), _sending_size);
            
            // start sending the remaining data
            _sending_data += bytes_transferred;
            _sending_size -= bytes_transferred;
            async_write_some(_sending_data, _sending_size);
        }
    }

    // Put an item into back of the pending queue.
    //
    // @return false if the pending queue is full.
    void put_into_pending_queue(
            const ReadBufferPtr& message,
            const SendCookie& cookie)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        ScopedLocker<FastLock> _(_pending_lock);
        _pending_calls.push_back(PendingItem(message, cookie));
        ++_pending_message_count;
        _pending_data_size += message->TotalCount();
        _pending_buffer_size += message->BlockCount() * TranBufPool::block_size();
    }

    // Insert an item into front of the pending queue.
    //
    // @return false if the pending queue is full.
    void insert_into_pending_queue(
            const ReadBufferPtr& message,
            const SendCookie& cookie)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        _swapped_calls.push_back(PendingItem(message, cookie));
        ++_swapped_message_count;
        _swapped_data_size += message->TotalCount();
        _swapped_buffer_size += message->BlockCount() * TranBufPool::block_size();
    }

    // Get an item from the pending queue.
    //
    // @return false if the pending queue is empty.
    bool get_from_pending_queue(
            ReadBufferPtr* message,
            SendCookie* cookie)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (_swapped_calls.empty() && _pending_message_count > 0)
        {
            // swap from _pending_calls
            ScopedLocker<FastLock> _(_pending_lock);
            if (!_pending_calls.empty())
            {
                _swapped_calls.swap(_pending_calls);
                _swapped_message_count = _pending_message_count;
                _swapped_data_size = _pending_data_size;
                _swapped_buffer_size = _pending_buffer_size;
                _pending_message_count = 0;
                _pending_data_size = 0;
                _pending_buffer_size = 0;
            }
        }

        if (!_swapped_calls.empty())
        {
            // fetch the top one
            *message = _swapped_calls.front().message;
            *cookie = _swapped_calls.front().cookie;
            _swapped_calls.pop_front();
            // update stats
            --_swapped_message_count;
            _swapped_data_size -= (*message)->TotalCount();
            _swapped_buffer_size -= (*message)->BlockCount() * TranBufPool::block_size();
            return true;
        }

        // no message found
        return false;
    }

    // Try to start receiving messages.  It will check that receiving is allowed,
    // and try to acquire the reveive token.
    //
    // If failed, the receiving is ignored.
    // If succeed, the token must be acquired.
    bool try_start_receive()
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (_receive_token == TOKEN_LOCK)
        {
            // no token
            return false;
        }

        if (_read_quota_token <= 0)
        {
            // last acquire quota failed
            return false;
        }

        if (_role_type == ROLE_TYPE_SERVER
                && _pending_buffer_size > _max_pending_buffer_size)
        {
            // sending buffer full, should suspend receiving to wait
            return false;
        }

        bool started = false;
        if (is_connected() 
                && atomic_comp_swap(&_receive_token, TOKEN_LOCK, TOKEN_FREE) == TOKEN_FREE)
        {
            SCHECK(_receiving_data != NULL);
            SCHECK(_receiving_size > 0);
            if((_read_quota_token = _flow_controller->acquire_read_quota(_receiving_size)) <= 0)
            {
                // no network quota
                atomic_comp_swap(&_receive_token, TOKEN_FREE, TOKEN_LOCK);
            }
            else
            {
                // now start receive
                async_read_some(_receiving_data, _receiving_size);
                started = true;
            }
        }
        return started;
    }

    // Try to start sending a message.  It will check that sending is allowed,
    // and try to acquire the send token.
    //
    // If failed, the sending is ignored.
    // If succeed, the token must be acquired.
    bool try_start_send()
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        if (_send_token == TOKEN_LOCK)
        {
            // no token
            return false;
        }

        if (_write_quota_token <= 0)
        {
            // last acquire quota failed
            return false;
        }

        bool started = false;
        while (is_connected() 
                && atomic_comp_swap(&_send_token, TOKEN_LOCK, TOKEN_FREE) == TOKEN_FREE)
        {
            if (get_from_pending_queue(&_sending_message, &_sending_cookie))
            {
                if (!on_sending(_sending_message, _sending_cookie))
                {
                    // need not send
                    on_send_failed(RPC_ERROR_REQUEST_CANCELED, _sending_message, _sending_cookie);
                    clear_sending_env();

                    atomic_comp_swap(&_send_token, TOKEN_FREE, TOKEN_LOCK);
                }
                else if ((_write_quota_token =_flow_controller->acquire_write_quota(
                                _sending_message->TotalCount())) <= 0)
                {
                    // no network quota
                    insert_into_pending_queue(_sending_message, _sending_cookie);
                    clear_sending_env();

                    atomic_comp_swap(&_send_token, TOKEN_FREE, TOKEN_LOCK);
                    break;
                }
                else
                {
                    // now send
                    _sent_size = 0;
                    bool ret = _sending_message->Next(
                            reinterpret_cast<const void**>(&_sending_data), &_sending_size);
                    SCHECK(ret);
                    async_write_some(_sending_data, _sending_size);
                    started = true;
                    break;
                }
            }
            else
            {
                // empty pending queue
                atomic_comp_swap(&_send_token, TOKEN_FREE, TOKEN_LOCK);
                break;
            }
        }
        return started;
    }

    // Process "data", split message, and store found messages.
    //
    // @return true if no error occured, and found messages are stored
    //              in "received_messages", may be empty.
    // @return false if error occured, for example bad message stream.
    class ReceivedItem;
    bool split_and_process_message(char* data, int size,
            std::deque<ReceivedItem>* received_messages)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        while (size > 0)
        {
            if (!_receiving_header_identified)
            {
                int consumed;
                int identify_result = identify_message_header(data, size, &consumed);
                if (identify_result > 0)
                {
                    _receiving_header_identified = true;
                    if (consumed == size)
                    {
                        // all the data is consumed by header
                        return true;
                    }
                    else
                    {
                        data += consumed;
                        size -= consumed;
                    }
                }
                else if (identify_result == 0)
                {
                    // in-complete header
                    return true;
                }
                else // identify_result < 0
                {
                    return false;
                }
            }

            int64 message_size = _receiving_header.message_size;
            if (_received_message_size + size < message_size)
            {
                // all the remaining data belongs to the in-complete message
                _receiving_message->Append(BufHandle(_tran_buf, size, data - _tran_buf));
                _received_message_size += size;
                return true;
            }

            int64 consume_size = message_size - _received_message_size;
            _receiving_message->Append(BufHandle(_tran_buf, consume_size, data - _tran_buf));
            received_messages->push_back(ReceivedItem(_receiving_message, 
                        _receiving_header.meta_size, _receiving_header.data_size));
            reset_receiving_env();
            data += consume_size;
            size -= consume_size;
        }

        return true;
    }

    // Identify a message header.
    //
    // @return 1  a header identified.  The header is stored in "_receiving_header",
    // @return 0  no header identified for incomplete data reason.  All the data is
    //            consumed and copied into "_receiving_header".
    // @return -1  the message stream is broken.
    int identify_message_header(char* data, int size, int* consumed)
    {
        // copy the data into the correct position of the header
        int header_size = static_cast<int>(sizeof(_receiving_header));
        int copy_size = std::min(size, header_size - _received_header_size);
        memcpy(reinterpret_cast<char*>(&_receiving_header) + _received_header_size,
                data, copy_size);
        _received_header_size += copy_size;
        *consumed = copy_size;

        if (_received_header_size < header_size)
        {
            // in-complete header
            return 0;
        }
        else // complete header
        {
            // check header magic str
            if (!_receiving_header.CheckMagicString())
            {
#if defined( LOG )
                LOG(ERROR) << "identify_message_header(): " << RpcEndpointToString(_remote_endpoint)
                           << ": check magic string failed: " << _receiving_header.magic_str_value;
#else
                SLOG(ERROR, "identify_message_header(): %s: check magic string failed: 0x%8x",
                        RpcEndpointToString(_remote_endpoint).c_str(), _receiving_header.magic_str_value);
#endif
                return -1;
            }
            if (_receiving_header.meta_size + _receiving_header.data_size != _receiving_header.message_size)
            {
#if defined( LOG )
                LOG(ERROR) << "identify_message_header(): " << RpcEndpointToString(_remote_endpoint)
                           << ": check size in header failed"
                           << ": meta_size=" << _receiving_header.meta_size
                           << ", data_size=" << _receiving_header.data_size
                           << ", message_size=" << _receiving_header.message_size;
#else
                SLOG(ERROR, "identify_message_header(): %s: "
                        "check size in header failed: meta_size=%d, data_size=%lld, message_size=%lld",
                        RpcEndpointToString(_remote_endpoint).c_str(),
                        _receiving_header.meta_size,
                        _receiving_header.data_size,
                        _receiving_header.message_size);
#endif
                return -1;
            }
            return 1;
        }
    }

    // Clear temp variables for sending message.
    void clear_sending_env()
    {
        _sending_message.reset();
        _sending_cookie = SendCookie();
        _sent_size = 0;
        _sending_data = NULL;
        _sending_size = 0;
    }

    // Reset temp variables for receiving message.
    void reset_receiving_env()
    {
        _receiving_message.reset(new ReadBuffer());
        _received_message_size = 0;
        _received_header_size = 0;
        _receiving_header_identified = false;
    }

    // Reset tran buf variables for receiving message.
    bool reset_tran_buf()
    {
        if (_tran_buf != NULL)
        {
            // free old buf, in fact just decrease ref count
            TranBufPool::free(_tran_buf);
            _tran_buf = NULL;
        }

        _tran_buf = reinterpret_cast<char*>(TranBufPool::malloc());
        if(_tran_buf == NULL)
        {
#if defined( LOG )
            LOG(ERROR) << "reset_tran_buf(): " << RpcEndpointToString(_remote_endpoint)
                       << ": malloc buffer failed: out of memory";
#else
            SLOG(ERROR, "reset_tran_buf(): %s: malloc buffer failed: out of memory",
                    RpcEndpointToString(_remote_endpoint).c_str());
#endif
            return false;
        }
        _receiving_data = reinterpret_cast<char*>(_tran_buf);
        _receiving_size = TranBufPool::block_size();
        return true;
    }

protected:
    RoleType _role_type;

private:
    struct PendingItem
    {
        ReadBufferPtr message;
        SendCookie cookie;
        PendingItem(const ReadBufferPtr& _message,
                    const SendCookie& _cookie)
            : message(_message)
            , cookie(_cookie) {}
    };

    struct ReceivedItem
    {
        ReadBufferPtr message;
        int meta_size;
        int64 data_size;
        ReceivedItem(const ReadBufferPtr& _message,
                     int _meta_size,
                     int64 _data_size)
            : message(_message)
            , meta_size(_meta_size)
            , data_size(_data_size) {}
    };

    // TODO improve sync queue performance
    volatile int _pending_message_count;
    volatile int64 _pending_data_size;
    volatile int64 _pending_buffer_size;
    std::deque<PendingItem> _pending_calls;
    FastLock _pending_lock;
    std::deque<PendingItem> _swapped_calls;
    volatile int _swapped_message_count;
    volatile int64 _swapped_data_size;
    volatile int64 _swapped_buffer_size;

    // flow control
    FlowControllerPtr _flow_controller;
    int64 _max_pending_buffer_size;
    volatile int32 _read_quota_token;  // <=0 means no quota
    volatile int32 _write_quota_token; // <=0 means no quota

    // statistics
    int64 _total_sent_count;
    int64 _total_sent_size;
    int64 _total_received_count;
    int64 _total_received_size;

    // temp variables for sending message
    ReadBufferPtr _sending_message; //current sending message
    SendCookie _sending_cookie; // cookie of the message
    int64 _sent_size; // current sent size of the message
    const char* _sending_data; // current sending data, weak ptr
    int _sending_size; // size of the current sending data

    // temp variables for receiving message
    ReadBufferPtr _receiving_message; // current receiving message
    int64 _received_message_size; // current received size of the message
    RpcMessageHeader _receiving_header;
    int _received_header_size;
    bool _receiving_header_identified;

    // tran buf for reading data
    char* _tran_buf; // strong ptr
    char* _receiving_data; // weak ptr
    int64 _receiving_size;

    // send and receive token
    static const int TOKEN_FREE = 0;
    static const int TOKEN_LOCK = 1;
    volatile int _send_token;
    volatile int _receive_token;
}; // class RpcMessageStream

} // namespace pbrpc
} // namespace trident

#endif // _SOFA_PBRPC_RPC_MESSAGE_STREAM_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
