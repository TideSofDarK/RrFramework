include(CTest)

enable_testing()

function(add_rr_test_executable TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "" "SOURCES")

    add_executable(${TARGET} ${TEST_SOURCES})
    target_link_libraries(${TARGET} PRIVATE RrFramework Vulkan::Headers)
    target_include_directories(${TARGET} SYSTEM PRIVATE ../Vendor)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

add_rr_test_executable(TestHashMap SOURCES TestHashMap.c)
