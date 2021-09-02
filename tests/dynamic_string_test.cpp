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
#ifdef USE_PMR_POOL_RESOURCE
#include <memory_resource>
#endif

#include "common.hpp"
#include "dynamic_string.hpp"

using namespace std::literals;
using namespace lserver;

class DynamicStringFixture : public ::testing::TestWithParam<std::size_t> {
public:
  DynamicStringFixture() { }

  void
  SetUp()
  {
    ds_ = new DynamicString{cap_, mr_};
  }

  void
  TearDown()
  {
    delete ds_;
  }

  ~DynamicStringFixture() { }

protected:
  std::pmr::unsynchronized_pool_resource mr_;
  DynamicString* ds_;
  std::size_t const cap_ = GetParam();
};

TEST_P(DynamicStringFixture, initial_state)
{
  EXPECT_EQ(ds_->capacity(), cap_);
  EXPECT_EQ(ds_->size(), 0);
}

TEST_P(DynamicStringFixture, initial_state_clear)
{
  ds_->clear();
  EXPECT_EQ(ds_->capacity(), cap_);
  EXPECT_EQ(ds_->size(), 0);
}

TEST_P(DynamicStringFixture, resize)
{
  ds_->resize(128);
  EXPECT_EQ(ds_->capacity(), 128);
  EXPECT_EQ(ds_->size(), 0);
}

TEST_P(DynamicStringFixture, fill)
{
  ds_->fill(cap_);
  EXPECT_EQ(ds_->capacity(), cap_);
  EXPECT_EQ(ds_->size(), cap_);
}

TEST_P(DynamicStringFixture, printf_0)
{
  constexpr char str[] = "ABCD";
  std::size_t len = strlen(str);

  ds_->printf(str);
  EXPECT_GE(ds_->capacity(), len);
  EXPECT_EQ(ds_->size(), len);
}

TEST_P(DynamicStringFixture, printf_1)
{
  constexpr char str[] = "ABCD";
  std::size_t len = strlen(str);

  ds_->printf("%s %d", str, 12);
  len += 3;
  EXPECT_GE(ds_->capacity(), len);
  EXPECT_EQ(ds_->size(), len);
}

INSTANTIATE_TEST_SUITE_P(T1, DynamicStringFixture, ::testing::Range<std::size_t>(0, 16));
