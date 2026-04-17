#ifndef PACKET_HPP
#define PACKET_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>



// Base class for all packets
class Packet {
    public:
        virtual ~Packet() = default;
        virtual void parse(const std::vector<uint8_t>& raw_data) = 0;
        virtual void print() const = 0;
};

// IMPU Packet structure (24 bytes total: 9 int16_t + 1 int16_t + 1 uint32_t) of sMouthguard data
class IMUPacket : public Packet {
    public:
        int16_t accel_x, accel_y, accel_z;
        int16_t gyro_x, gyro_y, gyro_z;
        int16_t magneto_x, magneto_y, magneto_z;
        int16_t temperature;
        uint32_t timestamp;

        void parse(const std::vector<uint8_t>& data) override {
            if (data.size() < 24) {
                throw std::runtime_error("Not enough data to parse IMU packet");
            }
            auto* s = reinterpret_cast<const int16_t*>(data.data());
            accel_x = s[0]; accel_y = s[1]; accel_z = s[2];
            gyro_x = s[3]; gyro_y = s[4]; gyro_z = s[5];
            magneto_x = s[6]; magneto_y = s[7]; magneto_z = s[8];
            temperature = s[9]; 
            std::memcpy(&timestamp, data.data() + 20, sizeof(uint32_t)); // Last 4 bytes for timestamp
            
        }

        void print() const override {
            std::cout << "IMU Packet - Accel: (" << accel_x << ", " << accel_y << ", " << accel_z 
                      << ") Gyro: (" << gyro_x << ", " << gyro_y << ", " << gyro_z 
                      << ") Magneto: (" << magneto_x << ", " << magneto_y << ", " << magneto_z 
                      << ") Temp: " << temperature 
                      << " Timestamp: " << timestamp 
                      << std::endl;
        }
};

class ESP32C3Packet : public Packet {
    public:
        // Define fields based on expected data structure from ESP32-C3
        void parse(const std::vector<uint8_t>& data) override {
            // Implement parsing logic based on the expected format of ESP32-C3 packets
        }

        void print() const override {
            // Implement printing logic for ESP32-C3 packet contents
        }
};  

// Factory to create packets based on UUID or other identifiers
class PacketFactory {
    public:
        // Constant for expected UUID of the sMouthguard's IMU data
        static constexpr std::string_view IMU_UUID = "0000xxxx-0000-1000-8000-00805f9b34fb"; // Replace with actual UUID
       
        static std::unique_ptr<Packet> create_packet(const std::vector<uint8_t>& raw_data, const std::string_view& uuid) {
            std::unique_ptr<Packet> packet = nullptr;

            if (uuid == IMU_UUID) {
               packet = std::make_unique<IMUPacket>();
            }

            if (packet) {
                packet->parse(raw_data);
            }
            // Add more packet types as needed
            return packet; // Unknown packet type
        }
};

#endif // PACKET_HPP