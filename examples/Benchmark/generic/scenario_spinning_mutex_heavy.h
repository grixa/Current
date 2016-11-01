/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef BENCHMARK_SCENARIO_SPINNING_MUTEX_HEAVY_H
#define BENCHMARK_SCENARIO_SPINNING_MUTEX_HEAVY_H

#include "benchmark.h"

#include "../../../Bricks/dflags/dflags.h"
#include "../../../Bricks/sync/spinning_mutex.h"
#include "../../../Bricks/util/sha256.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(mutex_heavy_test_type,
              "spinning",
              "The type of the mutex that is used to guard the calculations (std or spinning).");
DEFINE_string(mutex_heavy_test_string,
              "Not a very long text to generate SHA256 from.",
              "The test string to generate SHA256 from.");
#else
DECLARE_string(mutex_heavy_test_type);
DECLARE_string(mutex_heavy_test_string);
#endif

SCENARIO(spinning_mutex_heavy, "Test the SpinningMutex performance against the std::mutex (heavy calculations).") {
  template <typename MUTEX>
  class Sha256Calculator {
   public:
    Sha256Calculator(const std::string& data) : data_(data) {}
    void operator()() {
      auto v = current::SHA256(data_);
      {
        std::lock_guard<MUTEX> lock(mutex_);
        value_ = current::SHA256(data_);
        value2_.swap(v);
      }
    }

   private:
    const std::string data_;
    std::string value_;
    std::string value2_;
    MUTEX mutex_;
  };

  Sha256Calculator<std::mutex> sha_mutex_;
  Sha256Calculator<current::SpinningMutex> sha_spin_;
  std::function<void()> f;

  spinning_mutex_heavy() : sha_mutex_(FLAGS_mutex_heavy_test_string), sha_spin_(FLAGS_mutex_heavy_test_string) {
    if (FLAGS_mutex_heavy_test_type == "std") {
      f = [this]() { sha_mutex_(); };
    } else if (FLAGS_mutex_heavy_test_type == "spinning") {
      f = [this]() { sha_spin_(); };
    } else {
      std::cerr << "The `--mutex_heavy_test_type` flag must be \"std\" or \"spinning\"." << std::endl;
      CURRENT_ASSERT(false);
    }
  }

  void RunOneQuery() override { f(); }
};

REGISTER_SCENARIO(spinning_mutex_heavy);

#endif  // BENCHMARK_SCENARIO_SPINNING_MUTEX_HEAVY_H
