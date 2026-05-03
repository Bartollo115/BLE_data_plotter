#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "packet.hpp"

// ---------------------------------------------------------------------------
// ChannelBuffer — fixed-capacity ring buffer for one named scalar channel.
//
// ImPlot requires contiguous float arrays, so xs/ys are raw arrays that
// wrap around. ImPlot's ScrollingBuffer pattern is used: we pass the
// offset and let ImPlot handle the wrap.
// ---------------------------------------------------------------------------
struct ChannelBuffer {
    static constexpr int CAPACITY = 2000;   // ~33 seconds at 60Hz

    std::string name;
    bool        visible = true;             // toggled from the UI legend

    std::array<float, CAPACITY> xs{};       // time axis (seconds, host clock)
    std::array<float, CAPACITY> ys{};       // channel value
    int offset = 0;                         // current write head
    int count  = 0;                         // total samples written (caps at CAPACITY)

    void push(float x, float y);

    // Pointers and size passed directly to ImPlot::PlotLine
    const float* xData()  const { return xs.data(); }
    const float* yData()  const { return ys.data(); }
    int          plotOffset() const { return offset; }
    int          plotCount()  const { return std::min(count, CAPACITY); }
};

// ---------------------------------------------------------------------------
// DevicePlotBuffer — holds all channel buffers for one connected peripheral.
//
// Created when a peripheral is first seen; destroyed on disconnect.
// The render thread updates this from drained packets and reads from it
// during ImPlot drawing — both happen on the same thread so no locking needed.
// ---------------------------------------------------------------------------
class DevicePlotBuffer {
public:
    explicit DevicePlotBuffer(std::string deviceAddress,
                              std::string deviceType);

    // Ingest a packet — creates new ChannelBuffers if channels are new.
    void update(const Packet& packet);

    // Accessors for the render loop
    const std::string& address()    const { return address_; }
    const std::string& deviceType() const { return deviceType_; }

    // Returns channels in stable insertion order for consistent legend ordering
    const std::vector<std::string>&                   channelNames() const;
    const std::unordered_map<std::string, ChannelBuffer>& channelBuffers() const;

    ChannelBuffer*       channel(const std::string& name);
    const ChannelBuffer* channel(const std::string& name) const;

private:
    std::string address_;
    std::string deviceType_;

    // Ordered list of channel names (insertion order preserved for UI)
    std::vector<std::string>                      channelOrder_;
    std::unordered_map<std::string, ChannelBuffer> channels_;
};
