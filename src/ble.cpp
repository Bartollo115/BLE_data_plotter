#include "ble.hpp"

#include <chrono>

#include <SimpleBLE/SimpleBLE.h>

#include "logger.hpp"
#include "packet.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static double hostNow()
{
    using namespace std::chrono;
    static const auto epoch = steady_clock::now();
    return duration<double>(steady_clock::now() - epoch).count();
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
    static uint32_t rxCount = 0;
    if ((++rxCount % 100) == 0)
        LOG_DEBUG("SMouthguard rx #{} from {}", rxCount, address);

    auto packet = SMouthguardPacket::fromBytes(data);
    if (!packet) {
        LOG_WARN("SMouthguard malformed packet ({} bytes, expected {})",
                 data.size(), SMouthguardPacket::PACKET_SIZE);
        return;
    }
    packet->hostTimestamp = hostNow();
    packet->deviceAddress = address;
    queue_->push(std::move(packet));
}

bool SMouthguardPeripheral::subscribe(SimpleBLE::Peripheral& peripheral)
{
    LOG_INFO("Subscribing to SMouthguard {} ({})", name, address);

    for (auto& service : peripheral.services()) {
        for (auto& ch : service.characteristics()) {
            if (ch.uuid() != SMouthguardPacket::CHAR_UUID)
                continue;

            std::string svcUuid = service.uuid();
            std::string chUuid  = ch.uuid();

            LOG_INFO("Found IMU characteristic {} on service {}", chUuid, svcUuid);

            peripheral.notify(svcUuid, chUuid,
                [this, chUuid](SimpleBLE::ByteArray raw) {
                    onDataReceived(toBytes(raw), chUuid);
                });
            return true;
        }
    }

    LOG_WARN("IMU characteristic {} not found on {}", SMouthguardPacket::CHAR_UUID, address);
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
    LOG_WARN("ESP32C3 subscribe called but not yet implemented");
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
    else {
        LOG_DEBUG("Ignoring unknown peripheral: '{}' ({})", name, addr);
        return nullptr;
    }

    p->name    = std::string(name);
    p->address = std::string(addr);
    LOG_INFO("Created peripheral '{}' type={}", name, p->deviceType());
    return p;
}

// ---------------------------------------------------------------------------
// PeripheralManager
// ---------------------------------------------------------------------------
void PeripheralManager::add(std::unique_ptr<BLEPeripheral> peripheral)
{
    if (!peripheral)
        return;
    LOG_INFO("PeripheralManager: add {} ({})", peripheral->name, peripheral->address);
    std::lock_guard lock(mtx_);
    peripherals_.emplace(peripheral->address, std::move(peripheral));
}

void PeripheralManager::remove(std::string_view address)
{
    LOG_INFO("PeripheralManager: remove {}", address);
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
BLEManager::BLEManager(DataQueue* queue) : queue_(queue)
{
    LOG_INFO("BLEManager constructed");
}

BLEManager::~BLEManager()
{
    LOG_INFO("BLEManager shutting down");
    disconnectAll();
}

bool BLEManager::adapterAvailable() const
{
    std::lock_guard lock(mtx_);
    return adapter_ != nullptr;
}

bool BLEManager::startScan()
{
    auto adapters = SimpleBLE::Adapter::get_adapters();
    LOG_INFO("startScan: {} adapter(s) found", adapters.size());

    if (adapters.empty()) {
        LOG_ERROR("No Bluetooth adapters available");
        return false;
    }

    {
        std::lock_guard lock(mtx_);
        adapter_ = std::make_unique<SimpleBLE::Adapter>(std::move(adapters.front()));
    }

    LOG_INFO("Using adapter: {}", adapter_->identifier());

    adapter_->set_callback_on_scan_found(
        [this](SimpleBLE::Peripheral p) { onPeripheralFound(std::move(p)); });

    adapter_->scan_start();

    {
        std::lock_guard lock(mtx_);
        scanning_ = true;
    }

    LOG_INFO("Scan started");
    return true;
}

void BLEManager::stopScan()
{
    std::lock_guard lock(mtx_);
    if (adapter_ && scanning_) {
        adapter_->scan_stop();
        scanning_ = false;
        LOG_INFO("Scan stopped");
    }
}

bool BLEManager::isScanning() const
{
    std::lock_guard lock(mtx_);
    return scanning_;
}

void BLEManager::disconnectAll()
{
    LOG_INFO("Disconnecting all peripherals");
    stopScan();
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

    LOG_DEBUG("Scan found: '{}' ({})", name, addr);

    if (peripheralManager_.has(addr))
        return;

    auto device = PeripheralFactory::create(name, addr, queue_);
    if (!device)
        return;

    LOG_INFO("Connecting to '{}' ({})", name, addr);
    peripheral.connect();

    peripheral.set_callback_on_disconnected(
        [this, addr]() { onPeripheralDisconnected(addr); });

    if (!device->subscribe(peripheral)) {
        LOG_ERROR("Subscription failed for {} — disconnecting", addr);
        peripheral.disconnect();
        return;
    }

    LOG_INFO("'{}' ({}) connected and subscribed", name, addr);
    peripheralManager_.add(std::move(device));
}

void BLEManager::onPeripheralDisconnected(std::string_view address)
{
    LOG_WARN("Peripheral disconnected: {} — restarting scan", address);
    peripheralManager_.remove(address);

    // SimpleBLE won't re-fire scan_found for a known device unless the scan
    // is cycled. Restart it so the device can be rediscovered after reconnect.
    std::lock_guard lock(mtx_);
    if (adapter_ && scanning_) {
        adapter_->scan_stop();
        adapter_->scan_start();
        LOG_INFO("Scan restarted for rediscovery");
    }
}
