#include "packet.hpp"

#include <cstring>

#include "logger.hpp"

// AK09916 sensitivity in 16-bit mode
static constexpr float MAG_SCALE = 0.15f;   // µT / LSB

std::unique_ptr<SMouthguardPacket> SMouthguardPacket::fromBytes(
    const std::vector<uint8_t>& data)
{
    if (data.size() < PACKET_SIZE) {
        LOG_WARN("SMouthguardPacket::fromBytes: expected {} bytes, got {}",
                 PACKET_SIZE, data.size());
        return nullptr;
    }

    auto p = std::make_unique<SMouthguardPacket>();
    std::memcpy(&p->ax,          data.data() +  0, sizeof(int16_t));
    std::memcpy(&p->ay,          data.data() +  2, sizeof(int16_t));
    std::memcpy(&p->az,          data.data() +  4, sizeof(int16_t));
    std::memcpy(&p->gx,          data.data() +  6, sizeof(int16_t));
    std::memcpy(&p->gy,          data.data() +  8, sizeof(int16_t));
    std::memcpy(&p->gz,          data.data() + 10, sizeof(int16_t));
    std::memcpy(&p->mx,          data.data() + 12, sizeof(int16_t));
    std::memcpy(&p->my,          data.data() + 14, sizeof(int16_t));
    std::memcpy(&p->mz,          data.data() + 16, sizeof(int16_t));
    std::memcpy(&p->temperature, data.data() + 18, sizeof(int16_t));
    std::memcpy(&p->hwTimestamp, data.data() + 20, sizeof(uint32_t));
    return p;
}

std::vector<Channel> SMouthguardPacket::channels() const
{
    return {
        { "accel_x",   static_cast<float>(ax) * ACCEL_SCALE },
        { "accel_y",   static_cast<float>(ay) * ACCEL_SCALE },
        { "accel_z",   static_cast<float>(az) * ACCEL_SCALE },
        { "gyro_x",    static_cast<float>(gx) * GYRO_SCALE  },
        { "gyro_y",    static_cast<float>(gy) * GYRO_SCALE  },
        { "gyro_z",    static_cast<float>(gz) * GYRO_SCALE  },
        { "mag_x",     static_cast<float>(mx) * MAG_SCALE   },
        { "mag_y",     static_cast<float>(my) * MAG_SCALE   },
        { "mag_z",     static_cast<float>(mz) * MAG_SCALE   },
        { "temp_c",    static_cast<float>(temperature) / 333.87f + 21.0f },
    };
}

std::unique_ptr<Packet> PacketFactory::create_packet(
    const std::vector<uint8_t>& data,
    std::string_view charUuid)
{
    if (charUuid == SMouthguardPacket::CHAR_UUID) {
        auto p = SMouthguardPacket::fromBytes(data);
        if (!p)
            LOG_ERROR("PacketFactory: malformed SMouthguard payload ({} bytes)", data.size());
        return p;
    }
    LOG_DEBUG("PacketFactory: unrecognised UUID {}", charUuid);
    return nullptr;
}
