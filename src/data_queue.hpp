#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include "packet.hpp"

// ---------------------------------------------------------------------------
// DataQueue — thread-safe single-producer single-consumer queue.
//
// The BLE notification thread pushes packets; the ImGui render thread
// drains them at the start of each frame via drainInto().
// ---------------------------------------------------------------------------
class DataQueue {
public:
    // Push a packet from the BLE thread.
    // Never blocks — if the queue is at capacity the oldest packet is
    // dropped to prevent unbounded memory growth during a stall.
    void push(std::unique_ptr<Packet> packet);

    // Non-blocking pop. Returns nullptr if queue is empty.
    // Called by the render thread.
    std::unique_ptr<Packet> try_pop();

    // Drain up to maxItems packets into out in one lock acquisition.
    // Returns the number of packets moved.
    size_t drainInto(std::vector<std::unique_ptr<Packet>>& out,
                     size_t maxItems = 256);

    bool   empty() const;
    size_t size()  const;

    // Maximum number of packets held before oldest are dropped.
    static constexpr size_t MAX_QUEUE_DEPTH = 1024;

private:
    mutable std::mutex              mtx_;
    std::queue<std::unique_ptr<Packet>> queue_;
};
