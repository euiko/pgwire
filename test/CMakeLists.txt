add_executable(pgwire-test
    utils.cpp
)
target_link_libraries(pgwire-test PRIVATE Catch2WithMain pgwire)

include(CTest)
include(Catch)
catch_discover_tests(pgwire-test)
