//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beast-issues/2340
//

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <iostream>

// type aliases
namespace asio = boost::asio;
namespace beast = boost::beast;

using tcp = asio::ip::tcp;
using executor_type = asio::io_context::executor_type;
using stream_type = beast::basic_stream<tcp, executor_type>;
//using wss_stream_type=websock::stream<beast::ssl_stream<stream_type>, true>


asio::awaitable<tcp::resolver::results_type>
resolve()
{
    auto resolver = tcp::resolver(co_await asio::this_coro::executor);
    auto results = co_await resolver.async_resolve("localhost", "8080",
                                                   asio::use_awaitable);
    co_return results;
}

asio::awaitable<void>
run_test()
try
{
    auto exec = co_await asio::this_coro::executor;

    auto ws = beast::websocket::stream<tcp::socket>(exec);

    beast::websocket::permessage_deflate opt;
    opt.client_enable = true;// for clients
    opt.server_enable = true;// for servers
    ws.set_option(opt);

    auto endpoint = co_await async_connect(ws.next_layer(), co_await resolve(),
                           asio::use_awaitable);

    std::cout << "connected: " << endpoint << "\n";

    co_await ws.async_handshake("localhost", "/", asio::use_awaitable);

    static const char hello[] = "Hello, World!";

    co_spawn(exec, ws.async_write(asio::buffer(hello), asio::use_awaitable),
             asio::detached);

    beast::flat_buffer rxbuf;
    beast::error_code ec;
    while (!ec)
    {
        std::size_t bytes = co_await ws.async_read(
            rxbuf, asio::redirect_error(asio::use_awaitable, ec));
        if (ec) break;
        if (ws.got_text())
            std::cout << "response: " << beast::buffers_to_string(rxbuf.data())
                      << "\n";
        else
            std::cout << "response: <binary>\n";
        rxbuf.consume(bytes);
    }

    std::cout << "websocket closed: " << ec.message() << std::endl;
    std::cout << "websocket closed: " << ws.reason() << std::endl;

} catch (std::exception &e)
{
    std::cout << __func__ << ": exception: " << e.what() << "\n";
    throw;
}

int
main()
{
    auto ioc = asio::io_context();

    asio::co_spawn(ioc, run_test(), asio::detached);

    ioc.run();
}