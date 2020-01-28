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
#include "ngraph/file_util.hpp"
#include "ngraph/frontend/onnx_import/default_opset.hpp"
#include "ngraph/frontend/onnx_import/onnx.hpp"
#include "ngraph/opsets/opset0.hpp"
#include "ngraph/pass/manager.hpp"
#include "ngraph/pass/opset0_downgrade.hpp"
#include "ngraph/provenance.hpp"
#include "util/test_control.hpp"
#include "util/type_prop.hpp"

using namespace ngraph;
using namespace ngraph::onnx_import;

static std::string s_manifest = "${MANIFEST}";

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_tag_text)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_tag_add.prototxt"));

    const auto ng_nodes = function->get_ordered_ops();
    for (const auto ng_node : ng_nodes)
    {
        for (const auto tag : ng_node->get_provenance_tags())
        {
            EXPECT_HAS_SUBSTRING(tag, "ONNX");
        }
    }
}

// the NodeToCheck parameter of this template is used to find a node in the whole subgraph
// that a particular unit test is supposed to check against the expected provenance tag
template <typename NodeToCheck>
void test_provenance_tags(const std::shared_ptr<Function> function,
                          const std::string& expected_provenance_tag)
{
    int node_count = 0;
    for (const auto ng_node : function->get_ordered_ops())
    {
        if (as_type_ptr<NodeToCheck>(ng_node))
        {
            ++node_count;
            const auto tags = ng_node->get_provenance_tags();
            ASSERT_TRUE(tags.size() > 0) << "Node " << ng_node->get_friendly_name()
                                         << " should have at least one provenance tag.";
            EXPECT_TRUE(tags.find(expected_provenance_tag) != tags.end());
        }
    }
    EXPECT_TRUE(node_count > 0) << "Expected type of node doesn't exist in graph.";
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_only_output)
{
    // the Add node in the model does not have a name,
    // only its output name should be found in the provenance tags
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_only_outputs.prototxt"));
    test_provenance_tags<default_opset::Add>(function, "<ONNX Add (-> output_of_add)>");
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_node_name_and_outputs)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_node_name_and_outputs.prototxt"));
    test_provenance_tags<default_opset::Add>(function, "<ONNX Add (Add_node -> output_of_add)>");
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_multiple_outputs_op)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_multiple_outputs_op.prototxt"));
    test_provenance_tags<default_opset::TopK>(function, "<ONNX TopK (TOPK -> values, indices)>");
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_tagging_constants)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_input_tags.prototxt"));
    test_provenance_tags<default_opset::Constant>(function,
                                                  "<ONNX Input (initializer_of_A) Shape{0}>");
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_tagging_parameters)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_input_tags.prototxt"));
    test_provenance_tags<default_opset::Parameter>(function, "<ONNX Input (input_B) Shape{0}>");
}

NGRAPH_TEST(onnx_${BACKEND_NAME}, provenance_tag_downgrade_pass)
{
    set_provenance_enabled(true);

    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/provenance_downgrade_topk.prototxt"));

    ngraph::pass::Manager pass_manager;
    pass_manager.register_pass<pass::Opset0Downgrade>();
    pass_manager.run_passes(function);

    test_provenance_tags<op::v0::TopK>(function, "<ONNX TopK (TOPK -> values, indices)>");
    test_provenance_tags<op::v0::TopK>(function, "<Opset0_Downgrade (v1 TopK)>");
}
