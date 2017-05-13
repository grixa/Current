/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef BENCHMARK_SCENARIO_CHUNKED_HTTP_H
#define BENCHMARK_SCENARIO_CHUNKED_HTTP_H

#include "benchmark.h"

#include "../../../Blocks/HTTP/api.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_uint16(chunked_http_local_port, 9700, "Local port range for `current_http_server` to use.");
#else
DECLARE_uint16(chunked_http_local_port);
#endif

SCENARIO(current_chunked_http_large, "Use Current's HTTP stack for chunked HTTP get.") {
  std::string body_;
  std::string expected_response_;
  HTTPRoutesScope scope_;

  current_chunked_http_large() {
    std::string chunk(10, '.');
    for (size_t i = 0; i < 10000; ++i) {
      for (size_t j = 0; j < 10; ++j) {
        chunk[j] = 'A' + ((i + j) % 26);
      }
      body_ += chunk;
    }

    size_t count = std::count(body_.begin(), body_.end(), 'E');
    std::string count_string = current::ToString(count);
    expected_response_ = current::strings::Printf(
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/plain\r\n"
                             "Connection: close\r\n"
                             "Content-Length: %d\r\n"
                             "\r\n",
                             static_cast<int>(count_string.length())) +
                         count_string;

    const auto handler = [](Request r) { r(current::ToString(std::count(r.body.begin(), r.body.end(), 'E'))); };
    scope_ += HTTP(FLAGS_chunked_http_local_port).Register("/", handler);
  }

  void RunOneQuery() override {
    current::net::Connection connection(current::net::ClientSocket("localhost", FLAGS_chunked_http_local_port));
    connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
    connection.BlockingWrite("Host: localhost\r\n", true);
    connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
    connection.BlockingWrite("\r\n", true);
    connection.BlockingWrite(current::strings::Printf("%X\r\n", static_cast<int>(body_.length())), true);
    connection.BlockingWrite(body_, true);
    connection.BlockingWrite("0\r\n", false);
    std::vector<char> response(expected_response_.length() + 1);
    connection.BlockingRead(&response[0], expected_response_.length(), current::net::Connection::FillFullBuffer);
  }
};

REGISTER_SCENARIO(current_chunked_http_large);

SCENARIO(current_chunked_http_tiny, "Use Current's HTTP stack for chunked HTTP get.") {
  std::string chunked_body_;
  std::string expected_response_;
  HTTPRoutesScope scope_;

  current_chunked_http_tiny() {
    std::string chunk(10, '.');
    std::string body;
    for (size_t i = 0; i < 10000; ++i) {
      for (size_t j = 0; j < 10; ++j) {
        chunk[j] = 'A' + ((i + j) % 26);
      }
      chunked_body_ += "A\r\n" + chunk + "\r\n";
      body += chunk;
    }

    size_t count = std::count(body.begin(), body.end(), 'E');
    std::string count_string = current::ToString(count);
    expected_response_ = current::strings::Printf(
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/plain\r\n"
                             "Connection: close\r\n"
                             "Content-Length: %d\r\n"
                             "\r\n",
                             static_cast<int>(count_string.length())) +
                         count_string;

    const auto handler = [](Request r) { r(current::ToString(std::count(r.body.begin(), r.body.end(), 'E'))); };
    scope_ += HTTP(FLAGS_chunked_http_local_port).Register("/", handler);
  }

  void RunOneQuery() override {
    current::net::Connection connection(current::net::ClientSocket("localhost", FLAGS_chunked_http_local_port));
    connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
    connection.BlockingWrite("Host: localhost\r\n", true);
    connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
    connection.BlockingWrite("\r\n", true);
    connection.BlockingWrite(chunked_body_, true);
    connection.BlockingWrite("0\r\n", false);
    std::vector<char> response(expected_response_.length() + 1);
    connection.BlockingRead(&response[0], expected_response_.length(), current::net::Connection::FillFullBuffer);
  }
};

REGISTER_SCENARIO(current_chunked_http_tiny);

#endif  // BENCHMARK_SCENARIO_CHUNKED_HTTP_H
