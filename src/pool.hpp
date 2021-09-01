/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2021, Amin Saba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <stack>

#ifdef USE_TBB
#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#include <tbb/scalable_allocator.h>
#include <tbb/tbb.h>
#endif

#include "common.hpp"
#include "stats.hpp"

namespace lserver {

  using POI = uint64_t;
  constexpr POI POI_INVALID = UINT64_MAX;

  /*
   * Pool is a CRTP base for creating a dynamic pool of objects of type T.
   * Pool will call functions create(Args...) and destroy() in the derived
   * type D to create new instances of T and delete them respectively.
   *
   * @tparam T      The type of pooled object.
   * @tparam GArgs  The types of arguments passed to the creator of T
   */
  template <class T, class D, class... GArgs>
  class Pool {
#ifdef USE_TBB
    using TBBScalableAllocator = tbb::scalable_allocator<T*>;
    using Container = std::deque<T*, TBBScalableAllocator>;
#else
    using Container = std::deque<T*>;
#endif
    using GeneratorType = std::function<T*(GArgs...)>;

  public:
    Pool(std::size_t max_size, bool eager);
    ~Pool() noexcept;
    /*
     * Get an instance of T from the pool. This function always returns
     * immediately. If there are currently no free instances of T in the
     * pool, it creates a new instance and returns it.
     */
    T* borrow(GArgs... args);
    T* borrow(POI id, GArgs... args);
    /*
     * Get an instance of T from the pool. This function always returns
     * immediately. If there are currently no free instances of T in the
     * pool, the function returns nullptr and will later call 'callback'
     * with an instance as soon as it becomes available.
     */
    T* borrow(std::function<void(T*)> callback, GArgs... args);
    T* borrow(std::function<void(T*)> callback, POI id, GArgs... args);
    /*
     * Re-pool an instance of T so it can later be reused by other
     * requesters.
     */
    void put_back(T* p);
    /*
     * Forcefully recover and repool all "in-flight" items.
     * This calls finailze on all in-flight items.
     */
    void recover(POI id);
#ifdef ENABLE_STATISTICS
    /*
     * Get cumulative statistics of all T instance in the pool.
     */
    PoolStats const& get_stats() const;
    std::size_t get_size() const;
    std::size_t get_in_flight_cnt() const;
#endif

  protected:
    /*
     * Retruns a COPY of the the map containing items currently managed
     * by this pool.
     */
    auto all_items() const;

  private:
    /*
     * Returns a pointer to a newly generated item of type T
     */
    D* derived();
    /*
     * Create a new item of type T.
     */
    T* create(GArgs... args);
    /*
     * Destroys an already generated item.
     */
    void destroy(T* p);
    /*
     * subscribe()s and insert()s item p into the pool
     */
    void add(T* p);
    /*
     * Add p to the set of items managed/tracked by this pool.
     * This does not add p to the set of available items in the
     * pool.
     */
    void subscribe(T* p);
    /*
     * Extract an item from the pool and return, or if the pool
     * is empty it may use the create() method of the CRTP derivative
     * to generate a new item, if max item count limits allows that.
     * May return nullptr if the pool stack is empty and already
     * max_size_ items have been borrowed.
     */
    T* try_borrow(POI id, GArgs... args);
    /*
     * Addes p to the set of available items in the pool.
     */
    void insert(T* p);
    char const* get_derived_name();
    void print_name();

    mutable std::mutex mtx_;
    std::size_t max_size_;
    /*
     * A stack is used for tracking pooled items increase cache affinity.
     * Items are borrowed in a LIFO manner.
     */
    std::stack<T*, Container> pool_;
    /*
     * If 'callback_active_' is set, the first incomming item will be served
     * to a awaiting thread directly via this callback.
     * This callback is normally provided by the thread that has called borrow
     * and is asynchronously waiting to get an item.
     */
    std::function<void(T*)> callback_;
    /*
     * This is true if and only if there is a thread asynchronously waiting
     * to get a pooled item.
     */
    bool callback_active_ = false;
    /*
     * A map of all items created by, or subscribed to this pool.
     */
    std::unordered_map<T*, POI> all_items_;
    PoolStats stats_;
  };


  template <class T, class D, class... GArgs>
  inline Pool<T, D, GArgs...>::Pool(std::size_t max_size, bool eager)
      : max_size_{max_size}
  {
    if (max_size_ == 0 && eager)
      throw InvalidArgs{};

    if (eager) {
      for (std::size_t i = 0; i < max_size; ++i)
        add(create(GArgs{}...));
    }
  }

  template <class T, class D, class... GArgs>
  inline D*
  Pool<T, D, GArgs...>::derived()
  {
    return static_cast<D*>(this);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::create(GArgs... args)
  {
    return derived()->create(args...);
  }

  template <class T, class D, class... GArgs>
  inline void
  Pool<T, D, GArgs...>::destroy(T* p)
  {
    derived()->destroy(p);
  }

  template <class T, class D, class... GArgs>
  void
  Pool<T, D, GArgs...>::print_name()
  {
    constexpr bool has_name = requires(const D& d) { derived()->name(); };

    if constexpr (has_name)
      lslog_note(1, "Destroying", get_derived_name());
  }

  template <class T, class D, class... GArgs>
  Pool<T, D, GArgs...>::~Pool() noexcept
  {
    print_name();
    for (auto const& kv: all_items_)
      destroy(kv.first);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::borrow(GArgs... args)
  {
    return borrow(POI{}, args...);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::borrow(POI id, GArgs... args)
  {
    std::scoped_lock _{mtx_};
    auto p = try_borrow(id, args...);
    /*
     * If max_size_ is set to zero, (aka pool size is unlimited)
     * borrow always succeeds.
     */
    assert(max_size_ > 0 || p);
    return (p);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::borrow(std::function<void(T*)> callback, GArgs... args)
  {
    return borrow(callback, POI{}, args...);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::borrow(std::function<void(T*)> callback, POI id,
                               GArgs... args)
  {
    std::scoped_lock _{mtx_};
    auto p = try_borrow(id, args...);
    if (p != nullptr) [[likely]]
      callback(p);
    else {
      if (callback_active_) [[likely]]
        throw std::logic_error{"Invalid borrow request on a waiting pool"};
      callback_ = callback;
      callback_active_ = true;
    }
    return p;
  }

  template <class T, class D, class... GArgs>
  inline void
  Pool<T, D, GArgs...>::put_back(T* p)
  {
    assert(stats_.num_items_in_flight_ > 0);
    insert(p);
  }

#ifdef ENABLE_STATISTICS
  template <class T, class D, class... GArgs>
  PoolStats const&
  Pool<T, D, GArgs...>::get_stats() const
  {
    return stats_;
  }

  template <class T, class D, class... GArgs>
  std::size_t
  Pool<T, D, GArgs...>::get_size() const
  {
    return all_items_.size();
  }

  template <class T, class D, class... GArgs>
  std::size_t
  Pool<T, D, GArgs...>::get_in_flight_cnt() const
  {
    return stats_.num_items_in_flight_;
  }
#endif

  template <class T, class D, class... GArgs>
  inline void
  Pool<T, D, GArgs...>::add(T* p)
  {
    stats_.num_items_total_.fetch_add(1);
    subscribe(p);
    insert(p);
  }

  template <class T, class D, class... GArgs>
  inline void
  Pool<T, D, GArgs...>::subscribe(T* p)
  {
    all_items_.emplace(p, POI{});
    stats_.num_items_in_flight_.fetch_add(1);
    stats_.num_items_total_.fetch_add(1);
  }

  template <class T, class D, class... GArgs>
  inline T*
  Pool<T, D, GArgs...>::try_borrow(POI id, GArgs... args)
  {
    T* p = nullptr;

    if (!pool_.empty()) [[likely]] {
      p = pool_.top();
#if ENABLE_LS_SANITIZE
      assert(!p->engaged_);
      p->engaged_ = true;
#endif
      stats_.num_items_in_flight_.fetch_add(1);
      pool_.pop();
    } else {
      if (max_size_ == 0 || stats_.num_items_in_flight_.load() < max_size_) [[likely]] {
        p = create(args...);
        subscribe(p);
      }
    }

    if (p) [[likely]]
      all_items_.insert_or_assign(p, id);

    return p;
  }

  template <class T, class D, class... GArgs>
  inline void
  Pool<T, D, GArgs...>::insert(T* p)
  {
    assert(p);
#if ENABLE_LS_SANITIZE
    assert(p->engaged_);
    p->engaged_ = false;
#endif
    std::scoped_lock _{mtx_};
    if (callback_active_) {
      callback_(p);
      callback_active_ = false;
      return;
    }
    this->pool_.push(p);
    all_items_[p] = POI_INVALID;
    stats_.num_items_in_flight_.fetch_sub(1);
  }

  template <class T, class D, class... GArgs>
  auto
  Pool<T, D, GArgs...>::all_items() const
  {
    std::scoped_lock _{mtx_};
    return all_items_;
  }

  template <class T, class D, class... GArgs>
  char const*
  Pool<T, D, GArgs...>::get_derived_name()
  {
    return derived()->name();
  }

  template <class T, class D, class... GArgs>
  void
  Pool<T, D, GArgs...>::recover(POI id)
  {
    for (auto& kv: all_items())
      if (kv.second == id)
        kv.first->finalize();
  }

} // namespace lserver