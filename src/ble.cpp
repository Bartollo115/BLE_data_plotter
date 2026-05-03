#include "ble.hpp"

#include <chrono>

#include <SimpleBLE/SimpleBLE.h>

#include "packet.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static double hostNow()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static std::vector<uint8_t> toBytes(const SimpleBLE::ByteArray& ba)
{
    return { reinterpret_cast<const uint8_t*>(ba.data()),
             reinterpret_cast<const uint8_t*>(ba.data()) + ba.size() };
}

// ---------------------------------------------------------------------------
// SMouthguardPeripheral
// ---------------------------------------------------------------------------
SMouthguardPeripheral::SMouthguardPeripheral(DataQueue* queue)
{
    queue_ = queue;
}

void SMouthguardPeripheral::onDataReceived(const std::vector<uint8_t>& data,
                                           std::string_view /*charUuid*/)
{
    auto packet = SMouthguardPacket::fromBytes(data);
    if (!packet)
        return;
    packet->hostTimestamp = hostNow();
    packet->deviceAddress = address;
    queue_->push(std::move(packet));
}

bool SMouthguardPeripheral::subscribe(SimpleBLE::Peripheral& peripheral)
{
    for (auto& service : peripheral.services()) {
        for (auto& ch : service.characteristics()) {
            if (ch.uuid() != SMouthguardPacket::CHAR_UUID)
                continue;

            std::string svcUuid = service.uuid();
            std::string chUuid  = ch.uuid();
            std::string addr    = address;

            peripheral.notify(svcUuid, chUuid,
                [this, chUuid](SimpleBLE::ByteArray raw) {
                    onDataReceived(toBytes(raw), chUuid);
                });
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// ESP32C3Peripheral  (placeholder — wire up once Zephyr firmware is defined)
// ---------------------------------------------------------------------------
ESP32C3Peripheral::ESP32C3Peripheral(DataQueue* queue)
{
    queue_ = queue;
}

void ESP32C3Peripheral::onDataReceived(const std::vector<uint8_t>& /*data*/,
                                        std::string_view /*charUuid*/)
{
    // TODO: implement when ESP32-C3 packet format is finalised
}

bool ESP32C3Peripheral::subscribe(SimpleBLE::Peripheral& /*peripheral*/)
{
    // TODO: subscribe to ESP32-C3 notify characteristic
    return false;
}

// ---------------------------------------------------------------------------
// PeripheralFactory
// ---------------------------------------------------------------------------
std::unique_ptr<BLEPeripheral> PeripheralFactory::create(std::string_view name,
                                                          std::string_view addr,
                                                          DataQueue* queue)
{
    std::unique_ptr<BLEPeripheral> p;

    if (name.find("sMouthguard") != std::string_view::npos)
        p = std::make_unique<SMouthguardPeripheral>(queue);
    else if (name.find("ESP32-C3") != std::string_view::npos)
        p = std::make_unique<ESP32C3Peripheral>(queue);
    else
        return nullptr;

    p->name    = std::string(name);
    p->address = std::string(addr);
    return p;
}

// ---------------------------------------------------------------------------
// PeripheralManager
// ---------------------------------------------------------------------------
void PeripheralManager::add(std::unique_ptr<BLEPeripheral> peripheral)
{
    if (!peripheral)
        return;
    std::lock_guard lock(mtx_);
    peripherals_.emplace(peripheral->address, std::move(peripheral));
}

void PeripheralManager::remove(std::string_view address)
{
    std::lock_guard lock(mtx_);
    peripherals_.erase(std::string(address));
}

void PeripheralManager::handleNotification(const std::vector<uint8_t>& data,
                                            std::string_view charUuid,
                                            std::string_view address)
{
    std::lock_guard lock(mtx_);
    auto it = peripherals_.find(std::string(address));
    if (it != peripherals_.end())
        it->second->onDataReceived(data, charUuid);
}

bool PeripheralManager::has(std::string_view address) const
{
    std::lock_guard lock(mtx_);
    return peripherals_.contains(std::string(address));
}

std::vector<std::string> PeripheralManager::connectedAddresses() const
{
    std::lock_guard lock(mtx_);
    std::vector<std::string> out;
    out.reserve(peripherals_.size());
    for (auto& [addr, _] : peripherals_)
        out.push_back(addr);
    return out;
}

// ---------------------------------------------------------------------------
// BLEManager
// ---------------------------------------------------------------------------
BLEManager::BLEManager(DataQueue* queue) : queue_(queue) {}

BLEManager::~BLEManager()
{
    disconnectAll();
}

bool BLEManager::adapterAvailable() const
{
    std::lock_guard lock(mtx_);
    return adapter_.has_value();
}

bool BLEManager::startScan()
{
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty())
        return false;

    {
        std::lock_guard lock(mtx_);
        adapter_ = std::move(adapters.front());
    }

    adapter_->set_callback_on_scan_found(
        [this](SimpleBLE::Peripheral p) { onPeripheralFound(std::move(p)); });

    adapter_->scan_start();

    {
        std::lock_guard lock(mtx_);
        scanning_ = true;
    }
    return true;
}

void BLEManager::stopScan()
{
    std::lock_guard lock(mtx_);
    if (adapter_ && scanning_) {
        adapter_->scan_stop();
        scanning_ = false;
    }
}

bool BLEManager::isScanning() const
{
    std::lock_guard lock(mtx_);
    return scanning_;
}

void BLEManager::disconnectAll()
{
    stopScan();
    // SimpleBLE disconnects on Peripheral destruction; clearing the manager
    // drops all unique_ptrs which triggers peripheral disconnect via RAII.
    for (auto& addr : peripheralManager_.connectedAddresses())
        peripheralManager_.remove(addr);
}

std::vector<std::string> BLEManager::connectedDevices() const
{
    return peripheralManager_.connectedAddresses();
}

void BLEManager::onPeripheralFound(SimpleBLE::Peripheral peripheral)
{
    std::string name = peripheral.identifier();
    std::string addr = peripheral.address();

    if (peripheralManager_.has(addr))
        return;

    auto device = PeripheralFactory::create(name, addr, queue_);
    if (!device)
        return;

    peripheral.connect();

    peripheral.set_callback_on_disconnected(
        [this, addr]() { onPeripheralDisconnected(addr); });

    if (!device->subscribe(peripheral)) {
        peripheral.disconnect();
        return;
    }

    peripheralManager_.add(std::move(device));
}

void BLEManager::onPeripheralDisconnected(std::string_view address)
{
    peripheralManager_.remove(address);
}
