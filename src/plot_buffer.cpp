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

ImPlotPoint ChannelBuffer::getter(int idx, void* user_data)
{
    const auto* buf = static_cast<const ChannelBuffer*>(user_data);
    // Oldest sample sits at (offset - n) mod CAPACITY.
    // When buffer not yet full, offset == count so start == 0.
    int n     = buf->plotCount();
    int start = (buf->offset - n + CAPACITY * 2) % CAPACITY;
    int i     = (start + idx) % CAPACITY;
    return { static_cast<double>(buf->xs[i]),
             static_cast<double>(buf->ys[i]) };
}

// ---------------------------------------------------------------------------
// DevicePlotBuffer
// ---------------------------------------------------------------------------
DevicePlotBuffer::DevicePlotBuffer(std::string deviceAddress,
                                   std::string deviceType)
    : address_(std::move(deviceAddress))
    , deviceType_(std::move(deviceType))
{}

void DevicePlotBuffer::pushSample(const std::string& name, double t, float value)
{
    auto it = channels_.find(name);
    if (it == channels_.end()) {
        ChannelBuffer buf;
        buf.name = name;
        it = channels_.emplace(name, std::move(buf)).first;
        channelOrder_.push_back(name);
    }
    it->second.push(static_cast<float>(t), value);
    if (t > latestTime_)
        latestTime_ = t;
}

void DevicePlotBuffer::update(const Packet& packet)
{
    for (const auto& ch : packet.channels())
        pushSample(ch.name, packet.hostTimestamp, ch.value);
    latestTime_ = packet.hostTimestamp;
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
