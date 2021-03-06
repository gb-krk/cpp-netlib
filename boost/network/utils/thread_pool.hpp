#ifndef BOOST_NETWORK_UTILS_THREAD_POOL_HPP_20101020
#define BOOST_NETWORK_UTILS_THREAD_POOL_HPP_20101020

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <memory>
#include <functional>
#include <asio/io_service.hpp>
#include <boost/scope_exit.hpp>
#include <boost/network/utils/thread_group.hpp>

namespace boost {
namespace network {
namespace utils {

typedef std::shared_ptr<::asio::io_service> io_service_ptr;
typedef std::shared_ptr<utils::thread_group> worker_threads_ptr;
typedef std::shared_ptr<::asio::io_service::work> sentinel_ptr;

class thread_pool {
 public:
  thread_pool(thread_pool const &) = delete;
  thread_pool &operator=(thread_pool const &) = delete;
  thread_pool(thread_pool &&) noexcept = default;
  thread_pool &operator=(thread_pool &&) = default;

  thread_pool() : thread_pool(1) {}

  explicit thread_pool(std::size_t threads,
                       io_service_ptr io_service = io_service_ptr(),
                       worker_threads_ptr worker_threads = worker_threads_ptr())
      : threads_(threads),
        io_service_(std::move(io_service)),
        worker_threads_(std::move(worker_threads)),
        sentinel_() {
    bool commit = false;
    BOOST_SCOPE_EXIT_TPL(
        (&commit)(&io_service_)(&worker_threads_)(&sentinel_)) {
      if (!commit) {
        sentinel_.reset();
        io_service_.reset();
        if (worker_threads_.get()) {
          worker_threads_->join_all();
        }
        worker_threads_.reset();
      }
    }
    BOOST_SCOPE_EXIT_END

    if (!io_service_.get()) {
      io_service_.reset(new ::asio::io_service);
    }

    if (!worker_threads_.get()) {
      worker_threads_.reset(new utils::thread_group);
    }

    if (!sentinel_.get()) {
      sentinel_.reset(new ::asio::io_service::work(*io_service_));
    }

    for (std::size_t counter = 0; counter < threads_; ++counter) {
      worker_threads_->create_thread([=]() { io_service_->run(); });
    }

    commit = true;
  }

  std::size_t thread_count() const { return threads_; }

  void post(std::function<void()> f) { io_service_->post(f); }

  ~thread_pool() {
    sentinel_.reset();
    try {
      worker_threads_->join_all();
    } catch (const std::exception &) {
      assert(!"A handler was not supposed to throw, but one did.");
    }
  }

  void swap(thread_pool &other) {
    using std::swap;
    swap(other.threads_, threads_);
    swap(other.io_service_, io_service_);
    swap(other.worker_threads_, worker_threads_);
    swap(other.sentinel_, sentinel_);
  }

 protected:
  std::size_t threads_;
  io_service_ptr io_service_;
  worker_threads_ptr worker_threads_;
  sentinel_ptr sentinel_;
};

void swap(thread_pool &a, thread_pool &b) { a.swap(b); }
}  // namespace utils
}  // namespace network
}  // namespace boost

#endif /* BOOST_NETWORK_UTILS_THREAD_POOL_HPP_20101020 */
