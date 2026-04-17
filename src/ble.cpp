#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>
#include "../include/packet.hpp"

void dispatchToQueue(std::unique_ptr<Packet> packet) {
    // This function would push the packet into a thread-safe queue for processing
    // For example, you could use a std::queue with a mutex or a concurrent queue implementation
}
// BLEPeripheral base class
class BLEPeripheral {
    public:
        virtual ~BLEPeripheral() = default;
        virtual void onDataReceived(const std::vector<uint8_t>& data, const std::string_view charUuid) = 0;

        std::string address;
        std::string name;
};

class ESP32C3Peripheral : public BLEPeripheral {
    public:
        void onDataReceived(const std::vector<uint8_t>& data, const std::string_view charUuid) override {
            auto packet = PacketFactory::create_packet(data, charUuid);
            if (packet) {
                dispatchToQueue(std::move(packet));
            }
        }
};

class sMouthguardPeripheral : public BLEPeripheral {
    public:
        void onDataReceived(const std::vector<uint8_t>& data, const std::string_view charUuid) override {
            auto packet = PacketFactory::create_packet(data, charUuid);
            if (packet) {
                dispatchToQueue(std::move(packet));
            }
        }
};

class PeriperalFactory {
    public:
        static std::unique_ptr<BLEPeripheral> createPeripheral(const std::string_view& name, const std::string_view& address) {
            if (name.find("sMouthguard") != std::string_view::npos) {
                auto peripheral = std::make_unique<sMouthguardPeripheral>();
                peripheral->name = std::string(name);
                peripheral->address = std::string(address);
                return peripheral;
            }
            if (name.find("ESP32-C3") != std::string_view::npos) {
                auto peripheral = std::make_unique<ESP32C3Peripheral>();
                peripheral->name = std::string(name);
            }
            // Add more peripheral types as needed
            return nullptr; // Unknown peripheral type
        }
};

class PeripheralManager {
    public:
        void addPeripheral(std::unique_ptr<BLEPeripheral> peripheral) {
            if(peripheral)
            {
                peripherals[peripheral->address] = std::move(peripheral);
            }
        }

        // This would be called by the BLE library when data is received
        void handleDataReceived(const std::vector<uint8_t>& data,
                                const std::string_view charUuid,
                                const std::string_view peripheralAddress) {

            auto it = peripherals.find(std::string(peripheralAddress));
            if (it != peripherals.end()) {
                it->second->onDataReceived(data, charUuid);
            }
        }

        BLEPeripheral* getPeripheral(const std::string_view& address) {
            auto it = peripherals.find(std::string(address));
            if (it != peripherals.end()) {
                return it->second.get();
            }
            return nullptr; // Peripheral not found
        }
    private:
        std::unordered_map<std::string, std::unique_ptr<BLEPeripheral>> peripherals;
};

class BLEManager {
    private:
        PeripheralManager peripheralManager;

    public:
        void handleDiscoveredPeripheral(const std::string_view& name, const std::string_view& address) {
            auto newDevice = PeriperalFactory::createPeripheral(name, address);
            
            if (newDevice) {
                std::cout << "Discovered new peripheral: " << name << " at " << address << '\n';
                // Logic to connect to the peripheral and set up notifications would go here
                peripheralManager.addPeripheral(std::move(newDevice));
            }
        }
           
        void rawNotificationHandler(const std::vector<uint8_t>& data, const std::string_view& charUuid, const std::string_view& peripheralAddress) {
            peripheralManager.handleDataReceived(data, charUuid, peripheralAddress);
        }
};