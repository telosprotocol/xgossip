cmake_minimum_required(VERSION 3.8)

aux_source_directory(./src xgossip_src)
add_library(xgossip STATIC ${xgossip_src})

add_dependencies(xgossip xkad)
target_link_libraries(xgossip PRIVATE xkad xpbase xtransport xwrouter cpp_redis tacopie)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (XENABLE_CODE_COVERAGE)
        target_link_libraries(xgossip PRIVATE gcov)
    endif()
endif()
