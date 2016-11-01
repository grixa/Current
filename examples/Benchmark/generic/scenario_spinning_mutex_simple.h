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

#ifndef BENCHMARK_SCENARIO_SPINNING_MUTEX_SIMPLE_H
#define BENCHMARK_SCENARIO_SPINNING_MUTEX_SIMPLE_H

#include "../../../port.h"

#include "benchmark.h"

#include "../../../Bricks/dflags/dflags.h"
#include "../../../Bricks/sync/spinning_mutex.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(mutex_simple_test_type,
              "spinning",
              "The type of the mutex that is used to guard the calculations (std or spinning).");
#else
DECLARE_string(mutex_simple_test_type);
#endif

SCENARIO(spinning_mutex_simple, "Test the SpinningMutex performance against the std::mutex (simple calculations).") {
  template <typename MUTEX>
  class Incrementer {
   public:
    Incrementer() : protected_value_(0), unprotected_value_(1.0f) {}
    void operator()() {
      {
        std::lock_guard<MUTEX> lock(mutex_);
        ++protected_value_;
      }
      unprotected_value_ *= protected_value_;
    }

   private:
    uint64_t protected_value_;
    float unprotected_value_;
    MUTEX mutex_;
  };

  Incrementer<std::mutex> inc_mutex_;
  Incrementer<current::SpinningMutex> inc_spin_;
  std::function<void()> f;
  current::SpinningMutex outer_mutex_;

  spinning_mutex_simple() {
    if (FLAGS_mutex_simple_test_type == "std") {
      f = [this]() { inc_mutex_(); };
    } else if (FLAGS_mutex_simple_test_type == "spinning") {
      f = [this]() { inc_spin_(); };
    } else {
      std::cerr << "The `--mutex_simple_test_type` flag must be \"std\" or \"spinning\"." << std::endl;
      CURRENT_ASSERT(false);
    }
  }

  void RunOneQuery() override { f(); }
};

REGISTER_SCENARIO(spinning_mutex_simple);

#endif  // BENCHMARK_SCENARIO_SPINNING_MUTEX_SIMPLE_H
