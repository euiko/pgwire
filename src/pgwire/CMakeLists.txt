set(SOURCES
    buffer.cpp
    exception.cpp
    io.cpp
    log.cpp
    protocol.cpp
    server.cpp
    session.cpp
    types.cpp
    utils.cpp
    writer.cpp
)

if(PGWIRE_BUILD_SHARED OR BUILD_SHARED_LIBS)
    add_library(pgwire SHARED ${SOURCES})
else()
    add_library(pgwire STATIC ${SOURCES})
endif()

target_include_directories(pgwire PUBLIC ${INCLUDE_DIRECTORIES})
target_link_libraries(pgwire PUBLIC asio endian function2 promise)
