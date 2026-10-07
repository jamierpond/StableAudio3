#pragma once

#include <Pipeline/Pipeline.h>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace eacp::SA3App
{
enum class Stage
{
    Loading,
    Ready,
    Sampling,
    Decoding,
    Done,
    Failed
};

struct Status
{
    Stage stage = Stage::Loading;
    int step = 0;
    int steps = 0;
    std::string error;
    std::string footprint;
};

struct Result
{
    FilePath wav;
    std::vector<std::uint8_t> peaks;
};

// Loads the small model on a thread of its own as soon as it is built and runs
// every generation on that same thread. The callbacks arrive on the main
// thread.
class Generator
{
public:
    Generator();
    ~Generator();

    // Ignored while a generation is running.
    void generate(const SA3Pipeline::Request& request);

    std::function<void(const Status&)> onStatus = [](const Status&) {};
    std::function<void(const Result&)> onResult = [](const Result&) {};

    static constexpr int peakCount = 512;

private:
    void run(std::stop_token stopToken);
    std::optional<SA3Pipeline::Request> waitForRequest(std::stop_token stopToken);
    void runRequest(SA3Pipeline::Model& model, SA3Pipeline::Request request);
    void report(const Status& status);
    void reportFootprint(Status status, const std::string& label);
    void deliver(Result result);

    std::mutex mutex;
    std::condition_variable_any wake;
    std::optional<SA3Pipeline::Request> pending;
    bool busy = false;

    std::shared_ptr<Generator*> self = std::make_shared<Generator*>(this);
    std::jthread worker;
};
} // namespace eacp::SA3App
