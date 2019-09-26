//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
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
#include "ngraph/pass/opset1_downgrade.hpp"
#include "ngraph/graph_util.hpp"
#include "ngraph/graph_util.hpp"
#include "ngraph/op/get_output_element.hpp"
#include "ngraph/op/pad.hpp"
#include "ngraph/op/product.hpp"
#include "ngraph/op/reduce_prod.hpp"
#include "ngraph/op/reduce_sum.hpp"
#include "ngraph/op/reshape.hpp"
#include "ngraph/op/sum.hpp"

using namespace std;
using namespace ngraph;

#define NGRAPH_OP(a, b) a,
enum class OP_TYPEID
{
#include "ngraph/op/op_tbl.hpp"
};
#undef NGRAPH_OP

#define NGRAPH_OP(a, b) {#a, OP_TYPEID::a},
static unordered_map<string, OP_TYPEID> typeid_map{
#include "ngraph/op/op_tbl.hpp"
};
#undef NGRAPH_OP

static OP_TYPEID get_typeid(shared_ptr<Node> node)
{
    OP_TYPEID type_id;
    auto it = typeid_map.find(node->description());
    if (it != typeid_map.end())
    {
        type_id = it->second;
    }
    else
    {
        throw unsupported_op("Unsupported op '" + node->description() + "'");
    }
    return type_id;
}
// END mapping to OP_TYPEID

bool pass::Opset1Downgrade::run_on_node(shared_ptr<Node> node)
{
    bool modified = false;

    size_t op_version = node->get_version();

    if (op_version == 0)
    {
        return modified;
    }

    NGRAPH_CHECK(op_version == 1,
                 "Op version 1 transformation pass failed for ",
                 *node,
                 ", only op version 1 operations expected. Op version ",
                 op_version,
                 " found.");

// Not all enumeration values explicitly handled in switch
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
    switch (get_typeid(node))
    {
    case OP_TYPEID::Pad:
    {
        auto tmp = dynamic_cast<const op::v1::Pad*>(node.get());
        auto replacement_node = make_shared<op::v0::Pad>(node->input(0).get_source_output(),
                                                         node->input(3).get_source_output(),
                                                         tmp->get_pads_begin(),
                                                         tmp->get_pads_end(),
                                                         tmp->get_pad_mode());

        replace_node(node, replacement_node);
        modified = true;
        break;
    }
    case OP_TYPEID::Product:
    {
        auto tmp = dynamic_cast<const op::v1::ReduceProd*>(node.get());
        auto replacement_node = make_shared<op::v0::Product>(node->input(0).get_source_output(),
                                                             node->input(1).get_source_output());
        if (tmp->get_keep_dims())
        {
            NGRAPH_CHECK(tmp->reduction_axes_constant(),
                "Unable to convert ReduceProd:v1 to Product:v0 "
                "if reduction axes are not constant (for keep_dims=true). Node: ",
                *node);
            auto output_pshape = replacement_node->get_output_partial_shape(0);
            NGRAPH_CHECK(output_pshape.is_static(),
                         "Unable to convert ReduceProd:v1 to Product:v0 "
                         "if output shape is dynamic (for keep_dims=true). Node: ",
                         *node);
            const auto output_shape = output_pshape.to_shape();
            auto reshaped_output_shape = output_shape;
            for (const auto& axis : tmp->get_reduction_axes())
            {
                reshaped_output_shape.insert(reshaped_output_shape.begin() + axis, 1);
            }
            auto reshaped_product = make_shared<op::Reshape>(
                replacement_node->output(0), get_default_order(output_shape), reshaped_output_shape);
            replace_node(node, reshaped_product);
        }
        else
        {
            replace_node(node, replacement_node);
        }
        modified = true;
        break;
    }
    case OP_TYPEID::Sum:
    {
        bool keep_dims = false;
        auto replacement_node = make_shared<op::v1::ReduceSum>(
            node->input(0).get_source_output(), node->input(1).get_source_output(), keep_dims);
        replace_node(node, replacement_node);
        modified = true;
        break;
    }
    default: break;
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    return modified;
}