//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/beast-issue-2340
//

#ifndef BEAST_ISSUE_2340_TRASH_LOGGING_SOCKET_HPP
#define BEAST_ISSUE_2340_TRASH_LOGGING_SOCKET_HPP

struct logging_socket {
    using next_layer_type = tcp::socket;

    using executor_type = next_layer_type::executor_type;

    logging_socket(next_layer_type next)
        : next_layer_(std::move(next))
    {}

    executor_type
    get_executor()
    {
        return next_layer_.get_executor();
    }

    template<class MutableBufferSequence>
    struct read_op : asio::coroutine {
        MutableBufferSequence buf;
        logging_socket *ls;

        template<class Self>
        void operator()(Self &&self, beast::error_code ec = {},
                        std::size_t n = 0)
        {
            BOOST_ASIO_CORO_REENTER(this) for (;;)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    auto &s = ls->next_layer();
                    auto b = buf;
                    s.async_read_some(b, std::move(self));
                }

                std::cout << "(" << ec.message() << ", " << n << ") [read]: \n";
                self.complete(ec, n);
            }
        }
    };

    template<class ConstBufferSequence>
    struct write_op : asio::coroutine {
        ConstBufferSequence buf;
        logging_socket *ls;

        template<class Self>
        void operator()(Self &&self, beast::error_code ec = {},
                        std::size_t n = 0)
        {
            assert(self.get_executor()
                       .template target<asio::io_context::executor_type>());
            BOOST_ASIO_CORO_REENTER(this) for (;;)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    auto &s = ls->next_layer();
                    auto b = buf;
                    s.async_write_some(b, std::move(self));
                }

                std::cout << "(" << ec.message() << ", " << n
                          << ") [write]: \n";
                self.complete(ec, n);
            }
        }
    };

    template<class MutableBufferSequence,
             BOOST_ASIO_COMPLETION_HANDLER_FOR(
                 void(beast::error_code, std::size_t)) AsyncReadHandler>
    auto
    async_read_some(MutableBufferSequence buffer, AsyncReadHandler &&token)
    {
        return asio::async_compose<AsyncReadHandler,
                                   void(beast::error_code, std::size_t)>(
            read_op<MutableBufferSequence>{.buf = buffer, .ls = this}, token,
            asio::get_associated_executor(token, next_layer_.get_executor()));
    }

    template<class ConstBufferSequence,
             BOOST_ASIO_COMPLETION_HANDLER_FOR(
                 void(beast::error_code, std::size_t)) AsyncWriteHandler>
    auto
    async_write_some(ConstBufferSequence buffer, AsyncWriteHandler &&token)
    {
        return asio::async_compose<AsyncWriteHandler,
                                   void(beast::error_code, std::size_t)>(
            write_op<ConstBufferSequence>{.buf = buffer, .ls = this}, token,
            asio::get_associated_executor(token, next_layer_.get_executor()));
    }

    next_layer_type &
    next_layer()
    {
        return next_layer_;
    }

    next_layer_type const &
    next_layer() const
    {
        return next_layer_;
    }

private:
    tcp::socket next_layer_;
};

template<class TeardownHandler>
void
async_teardown(beast::role_type role, logging_socket &socket,
               TeardownHandler &&handler)
{
    boost::ignore_unused(role, socket, handler);

    socket.next_layer().shutdown(asio::socket_base::shutdown_both);
}

#endif//BEAST_ISSUE_2340_TRASH_LOGGING_SOCKET_HPP
