/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../../bricks/exception.h"
#include "../../bricks/strings/util.h"

#ifndef CURRENT_UTILS_NGINX_EXCEPTIONS_H
#define CURRENT_UTILS_NGINX_EXCEPTIONS_H

namespace current {
namespace nginx {

struct NginxException : Exception {
  using Exception::Exception;
};

struct NginxConfigException : NginxException {
  using NginxException::NginxException;
};

struct PortAlreadyUsedException : NginxConfigException {
  PortAlreadyUsedException(uint16_t port)
      : NginxConfigException("Port " + current::ToString(port) + " was already used in this config.") {}
};

struct NginxManagerException : NginxException {
  using NginxException::NginxException;
};

struct CannotWriteConfigFileException : NginxManagerException {
  explicit CannotWriteConfigFileException(const std::string& filename)
      : NginxManagerException("Failed to write to the config file '" + filename + "'") {}
};

struct NginxReloadConfigFailedException : NginxManagerException {
  using NginxManagerException::NginxManagerException;
};

}  // namespace current::nginx
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_EXCEPTIONS_H
