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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

#include "basic_pool.hpp"
#include "common.hpp"
#include "pool.hpp"

using namespace lserver;

/*
 * Simple pooled mock type
 */
class TestItem {
public:
  MOCK_METHOD0(death, void());
  MOCK_METHOD0(finalize, void());

  virtual ~TestItem() { death(); }
};

/*
 * A type that can be passed as callback to borrow operation of pool
 */
class BorrowCallback {
public:
  MOCK_METHOD1(cb, void(TestItem*));
};

/*
 * Fxiture for an unlimited BasicPool instance.
 */
class PoolFixture : public ::testing::TestWithParam<std::size_t> {
protected:
  BasicPool<TestItem> pool_{0, false};
};

/*
 * Fxiture for a BasicPool instance of parameterized sizze
 */
class LimitedPoolFixture : public ::testing::TestWithParam<std::size_t> {
protected:
  BasicPool<TestItem> pool_{GetParam(), false};
};

/*
 * Fxiture for an eager BasicPool instance of parameterized sizze
 */
class EagerLimitedPoolFixture : public ::testing::TestWithParam<std::size_t> {
protected:
  BasicPool<TestItem> pool_{GetParam(), true};
};


TEST_P(PoolFixture, size_1)
{
  auto x = pool_.borrow();
  EXPECT_EQ(pool_.get_size(), 1);
  EXPECT_EQ(pool_.get_in_flight_cnt(), 1);
  EXPECT_CALL(*x, death()).Times(1);
}

TEST_P(PoolFixture, size_1_in_flight)
{
  auto x = pool_.borrow();
  EXPECT_EQ(pool_.get_size(), 1);
  EXPECT_EQ(pool_.get_in_flight_cnt(), 1);
  pool_.put_back(x);
  EXPECT_EQ(pool_.get_in_flight_cnt(), 0);
  EXPECT_CALL(*x, death()).Times(1);
}

TEST_P(EagerLimitedPoolFixture, eager)
{
  std::stack<TestItem*> items;

  EXPECT_EQ(pool_.get_size(), GetParam());
  EXPECT_EQ(pool_.get_in_flight_cnt(), 0);

  for (std::size_t i = 0; i < GetParam(); ++i) {
    auto item = pool_.borrow();
    EXPECT_CALL(*item, death()).Times(1);
    items.push(item);
  }

  for (std::size_t i = 0; i < GetParam(); ++i) {
    pool_.put_back(items.top());
    items.pop();
  }
}

TEST_P(PoolFixture, id_recover)
{
  constexpr int id = 234;

  auto x = pool_.borrow(id);
  EXPECT_CALL(*x, death()).Times(1);
  EXPECT_CALL(*x, finalize()).Times(1);
  pool_.recover(id);
}

TEST_P(PoolFixture, id_no_recover)
{
  constexpr int id = 234;

  auto x = pool_.borrow(id);
  EXPECT_CALL(*x, death()).Times(1);
  EXPECT_CALL(*x, finalize()).Times(0);
}

TEST_P(PoolFixture, id_recover_passive)
{
  constexpr int id = 234;

  auto x = pool_.borrow(id);
  EXPECT_CALL(*x, death()).Times(1);
  EXPECT_CALL(*x, finalize()).Times(0);
  pool_.recover(1);
}

TEST_P(PoolFixture, lifo_borrow)
{
  auto x1 = pool_.borrow();
  auto x2 = pool_.borrow();
  pool_.put_back(x1);
  pool_.put_back(x2);

  auto x3 = pool_.borrow();

  EXPECT_EQ(x3, x2);

  EXPECT_CALL(*x1, death()).Times(1);
  EXPECT_CALL(*x3, death()).Times(1);
}

TEST_P(LimitedPoolFixture, active_borrow_cb)
{
  constexpr int id = 234;
  BorrowCallback bcb;
  /*
   * This pool has GetParam() item.
   * First, borrow them all.
   */
  TestItem* x;

  for (std::size_t i = 0; i < GetParam(); ++i) {
    x = pool_.borrow();
    EXPECT_CALL(*x, death()).Times(1);
  }
  EXPECT_EQ(pool_.get_size(), GetParam());
  EXPECT_EQ(pool_.get_in_flight_cnt(), GetParam());

  /*
   * Block on the pool, and wait for an item to become available
   */
  EXPECT_NO_THROW(pool_.borrow([&bcb](TestItem* p) { bcb.cb(p); }, id));

  /*
   * Expect to get back 'x' again from the blocking borrow call
   */
  EXPECT_CALL(bcb, cb(x)).Times(1);
  pool_.put_back(x);
}

TEST_P(LimitedPoolFixture, nested_active_borrow_cb)
{
  constexpr int id = 234;
  BorrowCallback bcb;
  /*
   * This pool has GetParam() item.
   * First, borrow them all.
   */
  TestItem* x;

  for (std::size_t i = 0; i < GetParam(); ++i) {
    x = pool_.borrow();
    EXPECT_CALL(*x, death()).Times(1);
  }

  /*
   * Block on the pool, and wait for an item to become available
   */
  EXPECT_NO_THROW(pool_.borrow([](auto) {}));
  EXPECT_THROW(pool_.borrow([](auto) {}), std::exception);
}

INSTANTIATE_TEST_SUITE_P(T1, PoolFixture, ::testing::Range<std::size_t>(0, 2));
INSTANTIATE_TEST_SUITE_P(T2, LimitedPoolFixture,
                         ::testing::Range<std::size_t>(1, 40));
INSTANTIATE_TEST_SUITE_P(T3, EagerLimitedPoolFixture,
                         ::testing::Range<std::size_t>(1, 40));
