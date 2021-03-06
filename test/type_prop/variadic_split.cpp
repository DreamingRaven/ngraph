//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "util/type_prop.hpp"

using namespace std;
using namespace ngraph;

TEST(type_prop, variadic_split)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});
    const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {1});
    const auto splits = op::Constant::create<int64_t>(element::i64, Shape{2}, {2, 4});
    const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
    EXPECT_EQ(split->outputs().size(), 2);
    EXPECT_EQ(split->output(0).get_shape(), (Shape{2, 2}));
    EXPECT_EQ(split->output(1).get_shape(), (Shape{2, 4}));
    EXPECT_EQ(split->output(0).get_element_type(), element::i32);
    EXPECT_EQ(split->output(1).get_element_type(), element::i32);

    EXPECT_EQ(make_shared<op::v1::VariadicSplit>(
                  make_shared<op::Parameter>(element::i32, Shape{12, 6}),
                  op::Constant::create<int64_t>(element::i64, Shape{}, {-2}),
                  op::Constant::create<int64_t>(element::i64, Shape{3}, {7, -1, 2}))
                  ->output(1)
                  .get_shape(),
              (Shape{3, 6}));

    EXPECT_EQ(make_shared<op::v1::VariadicSplit>(
                  make_shared<op::Parameter>(element::i32, Shape{12, 1, 6}),
                  op::Constant::create<int64_t>(element::i64, Shape{1}, {2}),
                  op::Constant::create<int64_t>(element::i64, Shape{3}, {3, 1, 2}))
                  ->output(2)
                  .get_shape(),
              (Shape{12, 1, 2}));

    EXPECT_EQ(make_shared<op::v1::VariadicSplit>(
                  make_shared<op::Parameter>(element::i32, Shape{12, 6}),
                  op::Constant::create<int64_t>(element::i64, Shape{1}, {1}),
                  op::Constant::create<int64_t>(element::i64, Shape{2}, {6, 0}))
                  ->output(1)
                  .get_shape(),
              (Shape{12, 0}));
}

TEST(type_prop, variadic_split_splits_rank)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});

    try
    {
        const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {1});
        const auto splits = op::Constant::create<int64_t>(element::i64, Shape{1, 2}, {2, 4});
        const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
        FAIL() << "Split node was created with incorrect data.";
    }
    catch (const NodeValidationFailure& error)
    {
        EXPECT_HAS_SUBSTRING(error.what(),
                             std::string("Split lengths should be a 1-D tensor. Got 2 instead."));
    }
}

TEST(type_prop, variadic_split_incorrect_sum)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});

    try
    {
        const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {1});
        const auto splits = op::Constant::create<int64_t>(element::i64, Shape{2}, {1, 6});
        const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
        FAIL() << "Split node was created with incorrect data.";
    }
    catch (const NodeValidationFailure& error)
    {
        EXPECT_HAS_SUBSTRING(
            error.what(),
            std::string("Total length of splits: 7 must match the length of the chosen axis: 6"));
    }
}

TEST(type_prop, variadic_split_incorrect_axis)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});

    try
    {
        const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {-5});
        const auto splits = op::Constant::create<int64_t>(element::i64, Shape{2}, {2, 4});
        const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
        FAIL() << "Split node was created with incorrect data.";
    }
    catch (const ngraph_error& error)
    {
        EXPECT_HAS_SUBSTRING(
            error.what(), std::string("Parameter axis -5 out of the tensor rank range [-2, 1]."));
    }
}

TEST(type_prop, variadic_split_splits_invalid_negative)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});

    try
    {
        const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {1});
        const auto splits = op::Constant::create<int64_t>(element::i64, Shape{2}, {-2, 4});
        const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
        FAIL() << "Split node was created with incorrect data.";
    }
    catch (const NodeValidationFailure& error)
    {
        EXPECT_HAS_SUBSTRING(
            error.what(), std::string("Invalid value -2 in split lengths input. Should be >= -1."));
    }
}

TEST(type_prop, variadic_split_splits_multiple_negatives)
{
    const auto data = make_shared<op::Parameter>(element::i32, Shape{2, 6});

    try
    {
        const auto axis = op::Constant::create<int64_t>(element::i64, Shape{}, {1});
        const auto splits = op::Constant::create<int64_t>(element::i64, Shape{3}, {-1, -1, 3});
        const auto split = make_shared<op::v1::VariadicSplit>(data, axis, splits);
        FAIL() << "Split node was created with incorrect data.";
    }
    catch (const NodeValidationFailure& error)
    {
        EXPECT_HAS_SUBSTRING(error.what(),
                             std::string("Cannot infer split with multiple -1 values at 0 and 1"));
    }
}