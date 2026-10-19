#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "data_queue.hpp"
#include "packet.hpp"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
namespace SimpleBLE { class Adapter; class Peripheral; }


// ---------------------------------------------------------------------------
// BLEPeripheral — abstract base for a connected device.
//
// Subclasses know which characteristic UUIDs to subscribe to and how to
// identify themselves. onDataReceived is called from the SimpleBLE
// callback thread — it must not block.
// ---------------------------------------------------------------------------
class BLEPeripheral {
public:
    virtual ~BLEPeripheral() = default;

    virtual void onDataReceived(const std::vector<uint8_t>& data,
                                std::string_view charUuid) = 0;

    // Called after construction to subscribe to notify characteristics.
    // Returns false if subscription fails.
    virtual bool subscribe(SimpleBLE::Peripheral& peripheral) = 0;

    virtual std::string_view deviceType() const = 0;

    std::string address;
    std::string name;

protected:
    // Shared queue injected at construction — all peripherals push to the
    // same queue; the render thread disambiguates by packet.deviceType()
    DataQueue* queue_ = nullptr;
};

// ---------------------------------------------------------------------------
// SMouthguardPeripheral — handles the sMouthguard BLE device.
// Subscribes to the IMU notify characteristic (UUID in SMouthguardPacket).
// ---------------------------------------------------------------------------
class SMouthguardPeripheral final : public BLEPeripheral {
public:
    explicit SMouthguardPeripheral(DataQueue* queue);

    void onDataReceived(const std::vector<uint8_t>& data,
                        std::string_view charUuid) override;

    bool subscribe(SimpleBLE::Peripheral& peripheral) override;

    std::string_view deviceType() const override { return "sMouthguard"; }
};

// ---------------------------------------------------------------------------
// ESP32C3Peripheral — placeholder for the Zephyr BLE demo (v1.1).
// TODO: define characteristic UUID and packet format once Zephyr firmware
//       is written for the SuperMini board.
// ---------------------------------------------------------------------------
class ESP32C3Peripheral final : public BLEPeripheral {
public:
    explicit ESP32C3Peripheral(DataQueue* queue);

    void onDataReceived(const std::vector<uint8_t>& data,
                        std::string_view charUuid) override;

    bool subscribe(SimpleBLE::Peripheral& peripheral) override;

    std::string_view deviceType() const override { return "ESP32-C3"; }
};

// ---------------------------------------------------------------------------
// PeripheralFactory — maps device name patterns to concrete peripheral types.
// ---------------------------------------------------------------------------
class PeripheralFactory {
public:
    static std::unique_ptr<BLEPeripheral> create(std::string_view name,
                                                  std::string_view address,
                                                  DataQueue* queue);
};

// ---------------------------------------------------------------------------
// PeripheralManager — owns connected peripherals, routes notifications.
// ---------------------------------------------------------------------------
class PeripheralManager {
public:
    void add(std::unique_ptr<BLEPeripheral> peripheral);
    void remove(std::string_view address);

    void handleNotification(const std::vector<uint8_t>& data,
                            std::string_view charUuid,
                            std::string_view address);

    bool has(std::string_view address) const;

    // Returns all connected device addresses — used by UI to build device list
    std::vector<std::string> connectedAddresses() const;

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::unique_ptr<BLEPeripheral>> peripherals_;
};

// ---------------------------------------------------------------------------
// BLEManager — top-level BLE controller.
//
// Owns the adapter, runs scan, connects to compatible peripherals,
// and wires notifications into the shared DataQueue.
//
// Intended to run on a dedicated thread; all public methods are called
// from the render thread and must be thread-safe.
// ---------------------------------------------------------------------------
class BLEManager {
public:
    explicit BLEManager(DataQueue* queue);
    ~BLEManager();

    // Start scanning for compatible peripherals. Non-blocking.
    bool startScan();
    void stopScan();
    bool isScanning() const;

    // Disconnect all and clean up. Called on app shutdown.
    void disconnectAll();

    // Status exposed to UI
    std::vector<std::string> connectedDevices() const;
    bool                     adapterAvailable() const;

private:
    void onPeripheralFound(SimpleBLE::Peripheral peripheral);
    void onPeripheralDisconnected(std::string_view address);

    DataQueue*                         queue_;
    std::unique_ptr<SimpleBLE::Adapter> adapter_;   // complete type only needed in ble.cpp
    PeripheralManager                  peripheralManager_;
    mutable std::mutex                 mtx_;
    bool                               scanning_ = false;
};
