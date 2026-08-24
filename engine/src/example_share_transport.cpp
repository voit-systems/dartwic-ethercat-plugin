#include "example_share_transport.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <zmq.hpp>

namespace Example {

struct ExampleShareTransport::Impl {
    std::string node_name;
    std::string receive_endpoint;
    std::string send_endpoint;
    bool network_mode = false;
    ReceiveHandler receive;
    std::atomic<bool> running{false};
    std::atomic<uint64_t> received_count{0};
    mutable std::mutex state_mutex;
    nlohmann::json last_frame = nlohmann::json::object();
    zmq::context_t context{1};
    std::unique_ptr<zmq::socket_t> receive_socket;
    std::unique_ptr<zmq::socket_t> send_socket;
    std::mutex outbound_mutex;
    std::deque<std::string> outbound;
    std::thread io_thread;

    explicit Impl(const nlohmann::json& config)
        : node_name(config.value("node_name", std::string{"example-flight-computer"})),
          receive_endpoint(config.value("receive_endpoint", std::string{})),
          send_endpoint(config.value("send_endpoint", std::string{})),
          network_mode(!receive_endpoint.empty() && !send_endpoint.empty()) {}

    void ioLoop() {
        while (running.load()) {
            std::deque<std::string> outgoing;
            {
                std::scoped_lock lock(outbound_mutex);
                outgoing.swap(outbound);
            }
            for (const auto& encoded : outgoing) {
                try { send_socket->send(zmq::buffer(encoded), zmq::send_flags::none); } catch (...) {}
            }
            try {
                zmq::message_t message;
                if (receive_socket->recv(message, zmq::recv_flags::none) && receive) {
                    receive(nlohmann::json::parse(message.to_string()));
                }
            } catch (...) {
                if (!running.load()) break;
            }
        }
    }
};

ExampleShareTransport::ExampleShareTransport(nlohmann::json config)
    : impl_(std::make_unique<Impl>(config)) {}

ExampleShareTransport::~ExampleShareTransport() {
    stop();
}

void ExampleShareTransport::start(ReceiveHandler receive) {
    impl_->receive = std::move(receive);
    impl_->running.store(true);
    if (impl_->network_mode) {
        impl_->receive_socket = std::make_unique<zmq::socket_t>(impl_->context, ZMQ_PULL);
        impl_->send_socket = std::make_unique<zmq::socket_t>(impl_->context, ZMQ_PUSH);
        impl_->receive_socket->set(zmq::sockopt::linger, 0);
        impl_->receive_socket->set(zmq::sockopt::rcvtimeo, 20);
        impl_->send_socket->set(zmq::sockopt::linger, 0);
        impl_->send_socket->set(zmq::sockopt::sndtimeo, 20);
        impl_->receive_socket->bind(impl_->receive_endpoint);
        impl_->send_socket->bind(impl_->send_endpoint);
        impl_->io_thread = std::thread([this]() { impl_->ioLoop(); });
        return;
    }
    impl_->receive({
        {"version", 4}, {"type", "hello"}, {"node_name", impl_->node_name},
        {"channels", nlohmann::json::array({{
            {"channel", "telemetry_heartbeat"},
            {"channel_data", {{"value", 1.0}, {"timestamp", uint64_t{1}}, {"units", "count"}}}
        }})},
        {"events", nlohmann::json::array()}
    });
}

bool ExampleShareTransport::send(const nlohmann::json& frame) {
    if (!impl_->running.load()) return false;
    {
        std::scoped_lock lock(impl_->state_mutex);
        impl_->last_frame = frame;
    }
    impl_->received_count.fetch_add(1, std::memory_order_relaxed);
    if (impl_->network_mode) {
        std::scoped_lock lock(impl_->outbound_mutex);
        impl_->outbound.push_back(frame.dump());
        return true;
    }
    if (frame.value("type", std::string{}) == "rapid.channel.upsert" &&
        frame.value("owner_node", std::string{}) == impl_->node_name && impl_->receive) {
        auto committed = frame;
        committed["channel_data"] = {
            {"value", frame.value("value", 0.0)},
            {"timestamp", frame.value("timestamp", uint64_t{0})}
        };
        impl_->receive(std::move(committed));
    }
    return true;
}

void ExampleShareTransport::stop() {
    if (!impl_ || !impl_->running.exchange(false)) return;
    if (impl_->io_thread.joinable()) impl_->io_thread.join();
    impl_->receive_socket.reset();
    impl_->send_socket.reset();
    {
        std::scoped_lock lock(impl_->outbound_mutex);
        impl_->outbound.clear();
    }
    impl_->receive = {};
}

uint64_t ExampleShareTransport::receivedCount() const {
    return impl_->received_count.load(std::memory_order_relaxed);
}

nlohmann::json ExampleShareTransport::lastFrame() const {
    std::scoped_lock lock(impl_->state_mutex);
    return impl_->last_frame;
}

} // namespace Example
