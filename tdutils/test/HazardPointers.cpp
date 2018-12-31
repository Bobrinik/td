//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2019
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/utils/HazardPointers.h"
#include "td/utils/logging.h"
#include "td/utils/port/thread.h"
#include "td/utils/Random.h"
#include "td/utils/tests.h"

#include <atomic>

#if !TD_THREAD_UNSUPPORTED
TEST(HazardPointers, stress) {
  struct Node {
    std::atomic<std::string *> name_;
    char pad[64];
  };
  int threads_n = 10;
  std::vector<Node> nodes(threads_n);
  td::HazardPointers<std::string> hazard_pointers(threads_n);
  std::vector<td::thread> threads(threads_n);
  int thread_id = 0;
  for (auto &thread : threads) {
    thread = td::thread([&, thread_id] {
      auto holder = hazard_pointers.get_holder(thread_id, 0);
      for (int i = 0; i < 1000000; i++) {
        auto &node = nodes[td::Random::fast(0, threads_n - 1)];
        auto *str = holder.protect(node.name_);
        if (str) {
          CHECK(*str == "one" || *str == "twotwo");
        }
        holder.clear();
        if (td::Random::fast(0, 5) == 0) {
          std::string *new_str = new std::string(td::Random::fast(0, 1) == 0 ? "one" : "twotwo");
          if (node.name_.compare_exchange_strong(str, new_str, std::memory_order_acq_rel)) {
            hazard_pointers.retire(thread_id, str);
          } else {
            delete new_str;
          }
        }
      }
    });
    thread_id++;
  }
  for (auto &thread : threads) {
    thread.join();
  }
  LOG(ERROR) << "Undeleted pointers: " << hazard_pointers.to_delete_size_unsafe();
  CHECK(static_cast<int>(hazard_pointers.to_delete_size_unsafe()) <= threads_n * threads_n);
  for (int i = 0; i < threads_n; i++) {
    hazard_pointers.retire(i);
  }
  CHECK(hazard_pointers.to_delete_size_unsafe() == 0);
}
#endif  //!TD_THREAD_UNSUPPORTED
