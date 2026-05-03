#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Channel — a single named scalar value extracted from a packet.
// The UI uses this to dynamically build the plot legend without knowing
// the concrete packet type.
// ---------------------------------------------------------------------------
struct Channel {
    std::string name;   // e.g. "accel_x", "gyro_z", "temp"
    float       value;  // converted to float for ImPlot
};

// ---------------------------------------------------------------------------
// Packet — abstract base for all BLE device packets.
//
// Concrete subclasses know their own wire format and expose a uniform
// channel list. The render thread never sees the concrete type.
// ---------------------------------------------------------------------------
class Packet {
public:
    virtual ~Packet() = default;

    // Returns all scalar channels this packet carries.
    // Called once per packet by the plot buffer to update its ring buffers.
    virtual std::vector<Channel> channels() const = 0;

    // Human-readable device type for UI labels, e.g. "sMouthguard"
    virtual std::string_view deviceType() const = 0;

    // Wall-clock time the notification arrived on the host (seconds).
    // Set by the BLE layer before pushing to the queue.
    double      hostTimestamp = 0.0;
    std::string deviceAddress;   // BLE MAC address — set by BLEPeripheral before push
};

// ---------------------------------------------------------------------------
// SMouthguardPacket — 24-byte IMU characteristic (UUID 0x2BAD)
//
// Wire layout (little-endian, matching firmware SensorPacket):
//   [0- 5]  ax ay az      int16_t each  (raw accel, ±4g range)
//   [6-11]  gx gy gz      int16_t each  (raw gyro,  ±500 dps range)
//  [12-17]  mx my mz      int16_t each  (raw mag,   AK09916)
//  [18-19]  temperature   int16_t       (raw ADC counts)
//  [20-23]  timestamp     uint32_t      (TI-RTOS Clock_getTicks)
//
// Accel scale:  raw / 8192.0f  → g   (ICM-20948, ±4g, 16-bit)
// Gyro scale:   raw / 65.5f    → dps (ICM-20948, ±500dps, 16-bit)
// Temp formula: (raw / 333.87f) + 21.0f → °C  (from ICM-20948 datasheet)
// ---------------------------------------------------------------------------
class SMouthguardPacket final : public Packet {
public:
    // Full 128-bit characteristic UUID as seen by SimpleBLE.
    static constexpr std::string_view CHAR_UUID =
        "f0002bad-0451-4000-b000-000000000000";

    static constexpr size_t PACKET_SIZE = 24;

    int16_t  ax, ay, az;
    int16_t  gx, gy, gz;
    int16_t  mx, my, mz;
    int16_t  temperature;
    uint32_t hwTimestamp;   // firmware tick counter

    // Construct from a raw BLE notification payload.
    // Returns nullptr if data is the wrong length.
    static std::unique_ptr<SMouthguardPacket> fromBytes(
        const std::vector<uint8_t>& data);

    std::vector<Channel> channels() const override;
    std::string_view deviceType() const override { return "sMouthguard"; }

private:
    // Scale factors from ICM-20948 datasheet
    static constexpr float ACCEL_SCALE = 1.0f / 8192.0f;   // → g
    static constexpr float GYRO_SCALE  = 1.0f / 65.5f;     // → dps
};

// ---------------------------------------------------------------------------
// PacketFactory — dispatches on characteristic UUID to the right subclass.
//
// For v1.0 only sMouthguard is handled.
// Add new device types here as new peripherals are implemented.
// ---------------------------------------------------------------------------
class PacketFactory {
public:
    // Returns nullptr if the UUID is unrecognised or data is malformed.
    static std::unique_ptr<Packet> create_packet(
        const std::vector<uint8_t>& data,
        std::string_view charUuid);
};
