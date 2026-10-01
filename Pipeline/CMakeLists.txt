add_library(eacp-sa3-pipeline STATIC Pipeline.cpp)

target_include_directories(eacp-sa3-pipeline PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)

target_link_libraries(eacp-sa3-pipeline PUBLIC
        eacp-sa3-checkpoints
        eacp-sa3-text-encoder
        eacp-sa3-dit
        eacp-sa3-codec
        eacp-sa3-sampler)

set_default_target_setting(eacp-sa3-pipeline)
add_ide_sources(eacp-sa3-pipeline)
