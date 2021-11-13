#include <functional>
#include <numeric>
#include <vector>

#include "gtest/gtest.h"

#include "rxmesh/rxmesh_attribute.h"
#include "rxmesh/rxmesh_iterator.cuh"
#include "rxmesh/rxmesh_static.h"
#include "rxmesh/util/import_obj.h"
#include "rxmesh/util/math.h"
#include "rxmesh/util/report.h"
#include "rxmesh_test.h"

#include "query.cuh"

using namespace rxmesh;


inline uint32_t max_output_per_element(const RXMeshStatic& rxmesh, const Op& op)
{
    if (op == Op::EV) {
        return 2;
    } else if (op == Op::EF) {
        return rxmesh.get_max_edge_incident_faces();
    } else if (op == Op::FV || op == Op::FE) {
        return 3;
    } else if (op == Op::FF) {
        return rxmesh.get_max_edge_adjacent_faces();
    } else if (op == Op::VV || op == Op::VE || op == Op::VF) {
        return rxmesh.get_max_valence();
    } else {
        RXMESH_ERROR("calc_fixed_offset() Invalid op " + op_to_string(op));
        return -1u;
    }
}


TEST(RXMeshStatic, Oriented_VV)
{

    // Select device
    /*cuda_query(rxmesh_args.device_id, rxmesh_args.quite);

    std::vector<std::vector<dataT>>    Verts;
    std::vector<std::vector<uint32_t>> Faces;

    ASSERT_TRUE(
        import_obj(STRINGIFY(INPUT_DIR) "cube.obj", Verts, Faces, true));

    // Instantiate RXMesh Static
    RXMeshStatic rxmesh_static(Faces, rxmesh_args.quite);

    EXPECT_TRUE(rxmesh_static.is_closed())
        << " Can't generate oriented VV for input with boundaries";

    // input/output container
    RXMeshAttribute<uint32_t> input_container;
    input_container.init(rxmesh_static.get_num_vertices(),
                         1u,
                         rxmesh::LOCATION_ALL,
                         rxmesh::AoS,
                         false,
                         false);

    RXMeshAttribute<uint32_t> output_container;
    output_container.init(rxmesh_static.get_num_vertices(),
                          max_output_per_element(rxmesh_static, Op::VV) + 1,
                          rxmesh::LOCATION_ALL,
                          rxmesh::SoA,
                          false,
                          false);

    // launch box
    LaunchBox<256> launch_box;
    rxmesh_static.prepare_launch_box(Op::VV, launch_box, false, true);

    // launch query
    float tt = launcher(rxmesh_static.get_context(),
                        Op::VV,
                        input_container,
                        output_container,
                        launch_box,
                        true);


    // move containers to the CPU for testing
    output_container.move(rxmesh::DEVICE, rxmesh::HOST);
    input_container.move(rxmesh::DEVICE, rxmesh::HOST);

    RXMeshTest tester(true);
    EXPECT_TRUE(tester.run_query_verifier(
        rxmesh_static, Faces, Op::VV, input_container, output_container));

    // Make sure orientation is accurate
    // for the cube, all angle are either 45 or 90

    auto vector_length = [](const dataT x, const dataT y, const dataT z) {
        return std::sqrt(x * x + y * y + z * z);
    };

    auto dot = [](const std::vector<dataT>& u, const std::vector<dataT>& v) {
        return std::inner_product(
            std::begin(u), std::end(u), std::begin(v), 0.0);
    };

    for (uint32_t v = 0; v < rxmesh_static.get_num_vertices(); ++v) {

        uint32_t vertex = input_container(v);

        uint32_t v_0 = output_container(v, output_container(v, 0));
        for (uint32_t i = 1; i < output_container(v, 0); ++i) {

            uint32_t v_1 = output_container(v, i);

            std::vector<dataT> p1{Verts[vertex][0] - Verts[v_0][0],
                                  Verts[vertex][1] - Verts[v_0][1],
                                  Verts[vertex][2] - Verts[v_0][2]};

            std::vector<dataT> p2{Verts[vertex][0] - Verts[v_1][0],
                                  Verts[vertex][1] - Verts[v_1][1],
                                  Verts[vertex][2] - Verts[v_1][2]};
            dataT              dot_pro = dot(p1, p2);
            dataT              theta =
                std::acos(dot_pro / (vector_length(p1[0], p1[1], p1[2]) *
                                     vector_length(p2[0], p2[1], p2[2])));
            theta = (theta * 180) / 3.14159265;
            EXPECT_TRUE(std::abs(theta - 90) < 0.0001 ||
                        std::abs(theta - 45) < 0.0001);
            v_0 = v_1;
        }
    }


    input_container.release();
    output_container.release();*/
}

template <rxmesh::Op op,
          typename InputHandleT,
          typename OutputHandleT,
          typename InputAttributeT,
          typename OutputAttributeT>
void launcher(RXMeshStatic&     rxmesh,
              InputAttributeT&  input,
              OutputAttributeT& output,
              RXMeshTest&       tester,
              Report&           report,
              bool              oriented)
{

    // launch box
    constexpr uint32_t      blockThreads = 256;
    LaunchBox<blockThreads> launch_box;
    rxmesh.prepare_launch_box(op, launch_box, false, oriented);

    // test data
    TestData td;
    td.test_name   = op_to_string(op);
    td.num_threads = launch_box.num_threads;
    td.num_blocks  = launch_box.blocks;
    td.dyn_smem    = launch_box.smem_bytes_dyn;
    td.static_smem = launch_box.smem_bytes_static;

    float total_time = 0;
    CUDA_ERROR(cudaProfilerStart());
    GPUTimer timer;

    for (uint32_t itr = 0; itr < rxmesh_args.num_run; itr++) {

        // Reset input/output
        if constexpr (op == Op::VV || op == Op::VE || op == Op::VF) {
            rxmesh.for_each_vertex([&](const InputHandleT& handle) {
                input(handle) = InputHandleT();
            });
            rxmesh.for_each_vertex([&](const InputHandleT& handle) {
                for (uint32_t j = 0; j < output.get_num_attributes(); ++j) {
                    output(handle, j) = OutputHandleT();
                }
            });
        }
        // if input is an edge
        if constexpr (op == Op::EV || op == Op::EF) {
            rxmesh.for_each_edge([&](const InputHandleT& handle) {
                input(handle) = InputHandleT();
            });
            rxmesh.for_each_edge([&](const InputHandleT& handle) {
                for (uint32_t j = 0; j < output.get_num_attributes(); ++j) {
                    output(handle, j) = OutputHandleT();
                }
            });
        }
        // if input is a face
        if constexpr (op == Op::FV || op == Op::FE || op == Op::FF) {
            rxmesh.for_each_face([&](const InputHandleT& handle) {
                input(handle) = InputHandleT();
            });
            rxmesh.for_each_face([&](const InputHandleT& handle) {
                for (uint32_t j = 0; j < output.get_num_attributes(); ++j) {
                    output(handle, j) = OutputHandleT();
                }
            });
        }


        timer.start();
        query_kernel<blockThreads, op, InputHandleT, OutputHandleT>
            <<<launch_box.blocks, blockThreads, launch_box.smem_bytes_dyn>>>(
                rxmesh.get_context(), input, output, oriented);

        timer.stop();
        CUDA_ERROR(cudaDeviceSynchronize());
        CUDA_ERROR(cudaGetLastError());
        CUDA_ERROR(cudaProfilerStop());

        total_time += timer.elapsed_millis();
        td.time_ms.push_back(timer.elapsed_millis());
    }

    // move containers to the CPU for testing
    output.move_v1(rxmesh::DEVICE, rxmesh::HOST);
    input.move_v1(rxmesh::DEVICE, rxmesh::HOST);

    // verify
    bool passed /*= tester.run_query_verifier(
        rxmesh, Faces, ops_it, input_container, output_container)*/
        ;

    td.passed.push_back(passed);
    EXPECT_TRUE(passed) << "Testing: " << td.test_name;

    report.add_test(td);
    if (!rxmesh_args.quite) {
        RXMESH_TRACE(" {} {} time = {} (ms)",
                     td.test_name.c_str(),
                     (passed ? " passed " : " failed "),
                     total_time / float(rxmesh_args.num_run));
    }
}

TEST(RXMeshStatic, Queries)
{
    bool oriented = false;

    // Select device
    cuda_query(rxmesh_args.device_id, rxmesh_args.quite);

    std::vector<std::vector<dataT>>    Verts;
    std::vector<std::vector<uint32_t>> Faces;

    ASSERT_TRUE(
        import_obj(rxmesh_args.obj_file_name, Verts, Faces, rxmesh_args.quite));

    // RXMesh
    RXMeshStatic rxmesh(Faces, rxmesh_args.quite);


    // Report
    Report report;
    report = Report("QueryTest_RXMesh");
    report.command_line(rxmesh_args.argc, rxmesh_args.argv);
    report.device();
    report.system();
    report.model_data(rxmesh_args.obj_file_name, rxmesh);
    report.add_member("method", std::string("RXMesh"));


    // Tester to verify all queries
    ::RXMeshTest tester(true);
    EXPECT_TRUE(tester.run_ltog_mapping_test(rxmesh, Faces))
        << "Local-to-global mapping test failed";

    {
        // VV
        auto input  = rxmesh.add_vertex_attribute<VertexHandle>("input", 1);
        auto output = rxmesh.add_vertex_attribute<VertexHandle>(
            "output", rxmesh.get_max_valence());
        launcher<Op::VV, VertexHandle, VertexHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }


    {
        // VE
        auto input  = rxmesh.add_vertex_attribute<VertexHandle>("input", 1);
        auto output = rxmesh.add_vertex_attribute<EdgeHandle>(
            "output", rxmesh.get_max_valence());
        launcher<Op::VE, VertexHandle, EdgeHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }

    {
        // VF
        auto input  = rxmesh.add_vertex_attribute<VertexHandle>("input", 1);
        auto output = rxmesh.add_vertex_attribute<FaceHandle>(
            "output", rxmesh.get_max_valence());
        launcher<Op::VF, VertexHandle, FaceHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }


    {
        // EV
        auto input  = rxmesh.add_edge_attribute<EdgeHandle>("input", 1);
        auto output = rxmesh.add_edge_attribute<VertexHandle>("output", 2);
        launcher<Op::EV, EdgeHandle, VertexHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }

    {
        // EF
        auto input  = rxmesh.add_edge_attribute<EdgeHandle>("input", 1);
        auto output = rxmesh.add_edge_attribute<FaceHandle>(
            "output", rxmesh.get_max_edge_incident_faces());
        launcher<Op::EF, EdgeHandle, FaceHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }

    {
        // FV
        auto input  = rxmesh.add_face_attribute<FaceHandle>("input", 1);
        auto output = rxmesh.add_face_attribute<VertexHandle>("output", 3);
        launcher<Op::FV, FaceHandle, VertexHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }

    {
        // FE
        auto input  = rxmesh.add_face_attribute<FaceHandle>("input", 1);
        auto output = rxmesh.add_face_attribute<EdgeHandle>("output", 2);
        launcher<Op::FE, FaceHandle, EdgeHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }

    {
        // FF
        auto input  = rxmesh.add_face_attribute<FaceHandle>("input", 1);
        auto output = rxmesh.add_face_attribute<FaceHandle>(
            "output", rxmesh.get_max_edge_adjacent_faces() - 1);
        launcher<Op::FF, FaceHandle, FaceHandle>(
            rxmesh, *input, *output, tester, report, oriented);
        rxmesh.remove_attribute("input");
        rxmesh.remove_attribute("output");
    }
    // Write the report
    report.write(
        rxmesh_args.output_folder + "/rxmesh",
        "QueryTest_RXMesh_" + extract_file_name(rxmesh_args.obj_file_name));
}
