#include <iomanip>
#include <sstream>

#include <pgwire/exception.hpp>
#include <pgwire/log.hpp>
#include <pgwire/session.hpp>
#include <pgwire/utils.hpp>

namespace pgwire {

using QueryId = int64_t;
static std::atomic<QueryId> id_counter = 0;

static std::unordered_map<std::string, std::string> server_status = {
    {"server_version", "14"},     {"server_encoding", "UTF-8"},
    {"client_encoding", "UTF-8"}, {"DateStyle", "ISO"},
    {"TimeZone", "UTC"},
};

Session::Session(SessionID id, asio::ip::tcp::socket &&socket)
    : _id(id), _startup_done(false), _socket{std::move(socket)} {

      };

Session::~Session() = default;

void Session::set_handler(ParseHandler &&handler) {
    _handler = std::move(handler);
}

Promise Session::start() {
    return newPromise([this](Defer &defer) {
        newPromise([=](Defer &read_defer) {
            do_read(read_defer);
        }).fail([defer] { defer.resolve(); });
    });
}

SessionID Session::id() const { return _id; }

void Session::do_read(Defer defer) {
    this->read().then([=](FrontendMessagePtr message) {
        if (!message) {
            do_read(defer);
            return;
        }

        process_message(message)
            .then([=]() { do_read(defer); })
            .fail([=](SqlExceptionPtr e) {
                if (e->get_severity() == ErrorSeverity::Fatal) {
                    defer.reject(e);
                    return;
                }

                ErrorResponse error_responsse{
                    e->get_message(), e->get_sqlstate(), e->get_severity()};
                this->write(encode_bytes(error_responsse)).then([this] {
                    return this->write(encode_bytes(ReadyForQuery{}));
                });
                do_read(defer);
            })
            .fail([=] { defer.reject(); });
    });
}

Promise Session::process_message(FrontendMessagePtr msg) {
    switch (msg->type()) {
    case FrontendType::Invalid:
    case FrontendType::Startup:
        return this->write(encode_bytes(AuthenticationOk{}))
            .then([this] {
                Promise promise = resolve();
                for (auto const &it : server_status) {
                    std::string key = it.first;
                    std::string value = it.second;
                    promise = promise.then([this, key, value] {
                        return this->write(
                            encode_bytes(ParameterStatus{key, value}));
                    });
                }
                return promise;
            })
            .then(
                [this] { return this->write(encode_bytes(ReadyForQuery{})); });
    case FrontendType::SSLRequest:
        return this->write(encode_bytes(SSLResponse{}));
    case FrontendType::Query: {
        auto *q = static_cast<Query *>(msg.get());
        auto id = ++id_counter;
        try {
            // quote query to be logged
            std::string quoted;
            {
                std::stringstream ss;
                ss << std::quoted(q->query);
                quoted = string_escape_space(ss.str());
            }
            auto timer = timer_start();
            log::info("[session #%d] [query #%d] executing query %s", _id, id,
                      quoted.c_str());

            // use shared_ptr to extend PreparedStatement, so it can outlive
            // this function
            auto prepared =
                std::make_shared<PreparedStatement>((*_handler)(q->query));
            return this->write(encode_bytes(RowDescription{prepared->fields}))
                .then([this, prepared] {
                    Writer writer{prepared->fields.size()};
                    try {
                        prepared->handler(writer, {});
                    } catch (SqlException &e) {
                        return reject(
                            std::make_shared<SqlException>(std::move(e)));
                    }

                    return this->write(encode_bytes(writer))
                        .then([this, num_rows = writer.num_rows()] {
                            return this->write(encode_bytes(CommandComplete{
                                string_format("SELECT %lu", num_rows)}));
                        });
                })
                .then([this] {
                    return this->write(encode_bytes(ReadyForQuery{}));
                })
                .fail([this, id](SqlExceptionPtr e) {
                    log::info("[session #%d] [query #%d] query execution "
                              "failed, error = %s",
                              _id, id, e->what());
                })
                .finally([this, id, timer] {
                    auto elapsed = duration_string(timer.elapsed());
                    log::info(
                        "[session #%d] [query #%d] query done, elapsed = %s",
                        _id, id, elapsed.c_str());
                });
        } catch (SqlException &e) {
            return reject(std::make_shared<SqlException>(std::move(e)));
        }
        break;
    }
    case FrontendType::Terminate:
        return reject();
    case FrontendType::Bind:
    case FrontendType::Close:
    case FrontendType::CopyFail:
    case FrontendType::Describe:
    case FrontendType::Execute:
    case FrontendType::Flush:
    case FrontendType::FunctionCall:
    case FrontendType::Parse:
    case FrontendType::Sync:
    case FrontendType::GSSResponse:
    case FrontendType::SASLResponse:
    case FrontendType::SASLInitialResponse:
        // std::cerr << "message type still not handled, type="
        //           << int(msg->type()) << "tag=" << char(msg->tag())
        //           << std::endl;
        break;
    }

    return resolve();
}

static std::unordered_map<FrontendTag, std::function<FrontendMessage *()>>
    sFrontendMessageRegsitry = {
        {FrontendTag::Query, []() { return new Query; }},
        {FrontendTag::Terminate, []() { return new Terminate; }},
};

Promise Session::read() {
    // std::cerr << "reading startup=" << _startup_done << std::endl;
    if (!_startup_done) {
        return read_startup();
    }

    constexpr auto kHeaderSize = sizeof(MessageTag) + sizeof(int32_t);
    auto header = std::make_shared<Bytes>(kHeaderSize);
    auto body = std::make_shared<Bytes>();

    return io::async_read_exact(_socket, asio::buffer(*header))
        .then([=] {
            Buffer headerBuffer(std::move(*header));
            MessageTag tag = headerBuffer.get_numeric<MessageTag>();
            int32_t len = headerBuffer.get_numeric<int32_t>();
            std::size_t size =
                len - sizeof(int32_t); // to exclude it self length

            return resolve(tag, size);
        })
        .then([=](MessageTag tag, std::size_t size) {
            body->resize(size);
            return io::async_read_exact(_socket, asio::buffer(*body)).then([=] {
                auto it = sFrontendMessageRegsitry.find(FrontendTag(tag));
                if (it == sFrontendMessageRegsitry.end()) {
                    return resolve(FrontendMessagePtr(nullptr));
                }

                Buffer buff(std::move(*body));
                auto fn = it->second;
                auto message = FrontendMessagePtr(fn());
                message->decode(buff);

                return resolve(message);
            });
        });
}
Promise Session::read_startup() {
    auto lenBuf = std::make_shared<int32_t>(0);
    auto bytes = std::make_shared<Bytes>();
    return io::async_read_exact(
               _socket, asio::buffer(reinterpret_cast<uint8_t *>(lenBuf.get()),
                                     sizeof(int32_t)))
        .then([lenBuf] {
            int32_t len = endian::network::get<int32_t>(
                reinterpret_cast<uint8_t *>(lenBuf.get()));
            std::size_t size =
                len - sizeof(int32_t); // to exclude it self length
            return resolve(size);
        })
        .then([=](std::size_t size) {
            bytes->resize(size);
            return io::async_read_exact(_socket, asio::buffer(*bytes))
                .then([=]() {
                    Buffer buf{std::move(*bytes)};
                    auto msg = std::make_shared<StartupMessage>();
                    msg->decode(buf);

                    if (!msg->is_ssl_request)
                        _startup_done = true;

                    return resolve(FrontendMessagePtr(msg));
                });
        });
}

Promise Session::write(Bytes &&b) {
    // use shared_buffer to extend the lifetime of bytes
    // since it can outlive this function
    auto shared = std::make_shared<Bytes>(std::move(b));
    return io::async_write(_socket, asio::buffer(*shared)).finally([shared]() {
        // ensure the shared bytes is destroyed at the end
    });
}

} // namespace pgwire
