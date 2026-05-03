#include "plot_buffer.hpp"

// ---------------------------------------------------------------------------
// ChannelBuffer
// ---------------------------------------------------------------------------
void ChannelBuffer::push(float x, float y)
{
    xs[offset] = x;
    ys[offset] = y;
    offset = (offset + 1) % CAPACITY;
    if (count < CAPACITY)
        ++count;
}

// ---------------------------------------------------------------------------
// DevicePlotBuffer
// ---------------------------------------------------------------------------
DevicePlotBuffer::DevicePlotBuffer(std::string deviceAddress,
                                   std::string deviceType)
    : address_(std::move(deviceAddress))
    , deviceType_(std::move(deviceType))
{}

void DevicePlotBuffer::update(const Packet& packet)
{
    float t = static_cast<float>(packet.hostTimestamp);
    for (const auto& ch : packet.channels()) {
        auto it = channels_.find(ch.name);
        if (it == channels_.end()) {
            ChannelBuffer buf;
            buf.name = ch.name;
            channels_.emplace(ch.name, std::move(buf));
            channelOrder_.push_back(ch.name);
            it = channels_.find(ch.name);
        }
        it->second.push(t, ch.value);
    }
}

const std::vector<std::string>& DevicePlotBuffer::channelNames() const
{
    return channelOrder_;
}

const std::unordered_map<std::string, ChannelBuffer>&
DevicePlotBuffer::channelBuffers() const
{
    return channels_;
}

ChannelBuffer* DevicePlotBuffer::channel(const std::string& name)
{
    auto it = channels_.find(name);
    return it != channels_.end() ? &it->second : nullptr;
}

const ChannelBuffer* DevicePlotBuffer::channel(const std::string& name) const
{
    auto it = channels_.find(name);
    return it != channels_.end() ? &it->second : nullptr;
}
