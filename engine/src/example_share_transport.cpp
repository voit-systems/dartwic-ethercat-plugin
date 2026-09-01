#include "example_share_transport.h"
#include <dartwic/share/DARTWICShareJsonCodec.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <zmq.hpp>

namespace Example {
using namespace DARTWIC::Share;

struct ExampleShareTransport::Impl {
    std::string node_name, receive_endpoint, send_endpoint;
    bool network_mode = false;
    TransportCallbacks callbacks;
    std::atomic<bool> running{false};
    std::atomic<uint64_t> received_count{0}, next_request{1};
    mutable std::mutex state_mutex;
    nlohmann::json last_frame = nlohmann::json::object();
    zmq::context_t context{1};
    std::unique_ptr<zmq::socket_t> receive_socket, send_socket;
    std::mutex outbound_mutex, pending_mutex;
    std::mutex inbound_mutex;
    std::condition_variable pending_cv;
    std::condition_variable inbound_cv;
    std::deque<std::string> outbound;
    using InboundDelivery=std::variant<Request,Telemetry,Control>;
    struct InboundRequest { InboundDelivery delivery; std::string id; };
    std::deque<InboundRequest> inbound_requests;
    std::unordered_map<std::string, std::optional<Response>> pending;
    std::thread io_thread, request_thread;

    explicit Impl(const nlohmann::json& config)
        : node_name(config.value("node_name", std::string{"example-flight-computer"})),
          receive_endpoint(config.value("receive_endpoint", std::string{})),
          send_endpoint(config.value("send_endpoint", std::string{})),
          network_mode(!receive_endpoint.empty() && !send_endpoint.empty()) {}

    void record(const nlohmann::json& frame) {
        { std::scoped_lock lock(state_mutex); last_frame = frame; }
        received_count.fetch_add(1, std::memory_order_relaxed);
    }
    void enqueue(nlohmann::json frame) {
        record(frame);
        std::scoped_lock lock(outbound_mutex); outbound.push_back(frame.dump());
    }

    void receiveFrame(const nlohmann::json& frame) {
        auto delivery = JsonCodec::delivery(frame);
        if (delivery == "response") {
            const auto response = JsonCodec::decodeResponse(frame); const auto id = JsonCodec::requestId(frame);
            if (!response || id.empty()) return;
            { std::scoped_lock lock(pending_mutex); const auto found = pending.find(id); if (found == pending.end()) return; found->second = *response; }
            pending_cv.notify_all(); return;
        }
        if (delivery == "request") {
            const auto request = JsonCodec::decodeRequest(frame); const auto id = JsonCodec::requestId(frame);
            if (!request || id.empty()) return;
            { std::scoped_lock lock(inbound_mutex); inbound_requests.push_back({InboundDelivery{*request},id}); }
            inbound_cv.notify_one(); return;
        }
        if (delivery == "control") { if (const auto value=JsonCodec::decodeControl(frame);value){{std::scoped_lock lock(inbound_mutex);inbound_requests.push_back({InboundDelivery{*value},{}});}inbound_cv.notify_one();}return; }
        if (const auto value=JsonCodec::decodeTelemetry(frame);value){{std::scoped_lock lock(inbound_mutex);inbound_requests.push_back({InboundDelivery{*value},{}});}inbound_cv.notify_one();}
    }

    void ioLoop() {
        while (running.load()) {
            std::deque<std::string> outgoing;
            { std::scoped_lock lock(outbound_mutex); outgoing.swap(outbound); }
            for (const auto& encoded : outgoing) { try { send_socket->send(zmq::buffer(encoded), zmq::send_flags::none); } catch (...) {} }
            try { zmq::message_t message; if (receive_socket->recv(message, zmq::recv_flags::none)) receiveFrame(nlohmann::json::parse(message.to_string())); }
            catch (...) { if (!running.load()) break; }
        }
    }

    void requestLoop() {
        while (running.load()) {
            InboundRequest incoming;
            {
                std::unique_lock lock(inbound_mutex);
                inbound_cv.wait(lock,[&]{return !running.load()||!inbound_requests.empty();});
                if (!running.load()) break;
                incoming=std::move(inbound_requests.front());inbound_requests.pop_front();
            }
            if(const auto* request=std::get_if<Request>(&incoming.delivery)){
                Response response;
                try { response=callbacks.on_request?callbacks.on_request(*request)
                    :Response{Unsupported{"request","No custom Share request handler."}}; }
                catch(const std::exception& error){response=RemoteError{error.what(),"handler_error"};}
                catch(...){response=RemoteError{"Unknown custom Share handler failure.","handler_error"};}
                enqueue(JsonCodec::encodeResponse(response,*request,incoming.id));
            }else if(const auto* telemetry=std::get_if<Telemetry>(&incoming.delivery)){
                try{if(callbacks.on_telemetry)callbacks.on_telemetry(*telemetry);}catch(...){}
            }else if(const auto* control=std::get_if<Control>(&incoming.delivery)){
                try{if(callbacks.on_control)callbacks.on_control(*control);}catch(...){}
            }
        }
    }

    Response simulatedRequest(const Request& request) {
        return std::visit([&](const auto& value) -> Response {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ChannelUpsert>) {
                ChannelTelemetry telemetry; telemetry.kind = ChannelTelemetry::Kind::Upsert; telemetry.upsert = value;
                telemetry.upsert.owner_node = node_name; telemetry.upsert.channel_data["value"] = value.value;
                if (callbacks.on_telemetry) callbacks.on_telemetry(Telemetry{telemetry});
                return Success{};
            } else if constexpr (std::is_same_v<T, ChannelRemove> || std::is_same_v<T, ChannelBulkUpsert>) return Success{};
            else if constexpr (std::is_same_v<T, ChannelQuery>) return ChannelQueryResult{{ChannelSnapshot{node_name,"telemetry_heartbeat",{{"value",1.0},{"timestamp",uint64_t{1}},{"units","count"}}}}};
            else if constexpr (std::is_same_v<T, ArgusQuery>) return ArgusQueryResult{};
            else return ArgusActionResult{value.event_id, {{"recorded",true}}};
        }, request);
    }
};

ExampleShareTransport::ExampleShareTransport(nlohmann::json config) : impl_(std::make_unique<Impl>(config)) {}
ExampleShareTransport::~ExampleShareTransport() { stop(); }

void ExampleShareTransport::start(TransportCallbacks callbacks) {
    impl_->callbacks = std::move(callbacks); impl_->running.store(true);
    if (impl_->callbacks.on_state) impl_->callbacks.on_state(TransportState::Connected, {});
    if (impl_->network_mode) {
        impl_->receive_socket = std::make_unique<zmq::socket_t>(impl_->context,ZMQ_PULL);
        impl_->send_socket = std::make_unique<zmq::socket_t>(impl_->context,ZMQ_PUSH);
        impl_->receive_socket->set(zmq::sockopt::linger,0);impl_->receive_socket->set(zmq::sockopt::rcvtimeo,20);
        impl_->send_socket->set(zmq::sockopt::linger,0);impl_->send_socket->set(zmq::sockopt::sndtimeo,20);
        impl_->receive_socket->bind(impl_->receive_endpoint);impl_->send_socket->bind(impl_->send_endpoint);
        impl_->request_thread=std::thread([this]{impl_->requestLoop();});
        impl_->io_thread=std::thread([this]{impl_->ioLoop();}); return;
    }
    if (impl_->callbacks.on_control) impl_->callbacks.on_control(Control{Hello{impl_->node_name,"example-sim-session",4,{true,true,true,true,true,true},true}});
    ChannelTelemetry telemetry;telemetry.kind=ChannelTelemetry::Kind::Upsert;telemetry.upsert={impl_->node_name,"telemetry_heartbeat","value",1.0,uint64_t{1},{},{{"value",1.0},{"timestamp",uint64_t{1}},{"units","count"}}};
    if (impl_->callbacks.on_telemetry) impl_->callbacks.on_telemetry(Telemetry{telemetry});
}

Response ExampleShareTransport::request(const Request& request, std::chrono::milliseconds timeout) {
    if (!impl_->running.load()) throw DisconnectedError("Example Share transport is disconnected.");
    if (!impl_->network_mode) return impl_->simulatedRequest(request);
    const auto id="example:"+std::to_string(impl_->next_request.fetch_add(1));
    {std::scoped_lock lock(impl_->pending_mutex);impl_->pending.emplace(id,std::nullopt);} impl_->enqueue(JsonCodec::encodeRequest(request,id));
    std::unique_lock lock(impl_->pending_mutex);if(!impl_->pending_cv.wait_for(lock,timeout,[&]{return !impl_->running.load()||impl_->pending.at(id).has_value();})){impl_->pending.erase(id);throw RequestTimeoutError("Example Share transport request timed out.");}
    if(!impl_->running.load()){impl_->pending.erase(id);throw DisconnectedError("Example Share transport disconnected.");}auto response=std::move(*impl_->pending.at(id));impl_->pending.erase(id);return response;
}
void ExampleShareTransport::publish(const Telemetry& telemetry) { if(!impl_->running.load())return;auto frame=JsonCodec::encodeTelemetry(telemetry);if(impl_->network_mode)impl_->enqueue(std::move(frame));else impl_->record(frame); }
void ExampleShareTransport::control(const Control& control) { if(!impl_->running.load())return;auto frame=JsonCodec::encodeControl(control);if(impl_->network_mode)impl_->enqueue(std::move(frame));else impl_->record(frame); }
void ExampleShareTransport::stop() { if(!impl_||!impl_->running.exchange(false))return;impl_->pending_cv.notify_all();impl_->inbound_cv.notify_all();if(impl_->io_thread.joinable())impl_->io_thread.join();if(impl_->request_thread.joinable())impl_->request_thread.join();impl_->receive_socket.reset();impl_->send_socket.reset();if(impl_->callbacks.on_state)impl_->callbacks.on_state(TransportState::Disconnected,{});impl_->callbacks={}; }
uint64_t ExampleShareTransport::receivedCount() const{return impl_->received_count.load(std::memory_order_relaxed);} nlohmann::json ExampleShareTransport::lastFrame() const{std::scoped_lock lock(impl_->state_mutex);return impl_->last_frame;}

} // namespace Example
