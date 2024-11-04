#include "asio/ip/address.hpp"
#include <cmath>
#include <cstdlib>
#include <pgwire/log.hpp>
#include <pgwire/protocol.hpp>
#include <pgwire/server.hpp>
#include <pgwire/types.hpp>
#include <pgwire/writer.hpp>

#include <iostream>
#include <asio.hpp>

struct Options {
    std::string host;
    int port;
};

static bool parse_args(Options &options, int argc, char **argv);

int main(int argc, char **argv) {
    using namespace asio;

    // parse arguments
    Options options {
        "127.0.0.1",
        15432
    };
    if (!parse_args(options, argc, argv)) {
        return 1;
    }

    io_context io_context;
    auto address = ip::make_address(options.host);
    ip::tcp::endpoint endpoint(address, options.port);

    int64_t len = 1000;
    if (argc > 1) {
        len = atoll(argv[1]);
    }

    pgwire::log::initialize(io_context);
    pgwire::log::info("Starting server on %s:%d", options.host.c_str(), options.port);

    pgwire::Server server(io_context, endpoint, [len](pgwire::Session &sess) {
        return [len](std::string const &query) {
            pgwire::PreparedStatement stmt;
            stmt.fields = pgwire::Fields{
                {"name", pgwire::Oid::Text},
                {"address", pgwire::Oid::Text},
                {"age", pgwire::Oid::Int8},
            };
            stmt.handler = [len](pgwire::Writer &writer,
                                 pgwire::Values const &parameters) {
                for (int i = 1; i <= len; i++) {
                    auto row = writer.add_row();
                    row.write_string("kharista");
                    row.write_string("indonesia");
                    row.write_int8(i);
                }
            };
            return stmt;
        };
    });
    server.start();
    io_context.run();
    return 0;
}

bool parse_args(Options &options, int argc, char **argv) {
    // avoid any parsing if no arguments passed
    if (argc < 2) {
        return true;
    }

    char const* program = argv[0];
    int cursor = 1; // exclude program name
    auto pop = [&](char const* &value){
        if (cursor < argc) {
            value = argv[cursor];
            cursor++;

            return true;
        }

        return false;
    };

    char const* arg = nullptr;
    while (pop(arg)) {
        std::string args(arg);

        if (args == "--host" || args == "-H") {
            char const* host = nullptr;
            if (!pop(host)) {
                std::cerr << "Please specify the <host> to listen on\n";
                goto usage;
            }

            options.host = host;
        } else if ( args == "--port" || args == "-P") {
            char const* port = nullptr;
            if (!pop(port)) {
                std::cerr << "Please specify the <port> to listen on\n";
                goto usage;
            }

            int result = atoi(port);
            if (result < 1 || result > 65535) {
                std::cerr << "Please specify a valid <port> (between 1-65535)\n";
                goto usage;
            }

            options.port = result;
        } else if (args == "--help" || args == "-h") {
            goto usage;
        } else {
            std::cerr << "Unknown arguments: " << args << std::endl;
            goto usage;
        }
    }

    return true;

    usage:
    std::cerr << "pgwire-demo is a demo program to showcase the usage of pgwire library" << std::endl;
    std::cerr << "Usage: " << program << " [OPTIONS]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Available options:" << std::endl;
    std::cerr << "  --host (-H)        Host to listen on (default: 127.0.0.1)\n";
    std::cerr << "  --port (-P)        Port to listen on (default: 15432)\n";
    std::cerr << "  --help (-h)        To show this usage\n";
    return false;
}
