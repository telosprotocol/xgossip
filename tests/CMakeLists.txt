aux_source_directory(./ xgossip_test_dir)

add_executable(xgossip_test ${xgossip_test_dir})

add_dependencies(xgossip_test xgossip xxbase)

target_link_libraries(xgossip_test
    xgossip 
    xkad
    xtransport
    xpbase
    xledger
    xcrypto
    xutility
    xxbase
    common
    protobuf
    -lgtest
    -lgmock
    -lpthread -ldl
    -lrt
)

