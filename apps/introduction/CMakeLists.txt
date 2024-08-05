add_executable(introduction)

set(SOURCE_LIST
    introduction.cu	
)

target_sources(introduction
    PRIVATE
    ${SOURCE_LIST}
)

set_target_properties(introduction PROPERTIES FOLDER "apps")

set_property(TARGET ARAP PROPERTY CUDA_SEPARABLE_COMPILATION ON)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "introduction" FILES ${SOURCE_LIST})

target_link_libraries(introduction
    PRIVATE RXMesh
)

#gtest_discover_tests( introduction )
