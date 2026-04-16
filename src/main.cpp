#include <SimpleBLE/SimpleBLE.h>
#include <imgui.h>
#include <implot.h>
#include <vector>
#include <mutex>

// Use a struct to keep telemetry data organized
struct TelemetryData {
    std::vector<float> time;
    std::vector<float> values;
    std::mutex mtx; // Protects the vectors from concurrent access
};

TelemetryData shared_data;

void ble_setup() {
    // This would ideally run in its own thread or via SimpleBLE callbacks
    auto adapter = SimpleBLE::Adapter::get_adapters()[0];
    
    adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral) {
        if (peripheral.identifier() == "ESP32-C3-Sensor") {
            peripheral.connect();
            // Start notifying...
        }
    });
}

int main() {
    // 1. Initialize GUI Contexts (You've done this)
    // 2. Initialize BLE Setup
    
    while (app_is_running) {
        // --- START UI FRAME ---
        
        ImGui::Begin("Live Telemetry");

        // Use the Mutex to safely read data for plotting
        {
            std::lock_guard<std::mutex> lock(shared_data.mtx);
            if (ImPlot::BeginPlot("Sensor Output")) {
                ImPlot::PlotLine("Channel 1", shared_data.time.data(), 
                                 shared_data.values.data(), shared_data.values.size());
                ImPlot::EndPlot();
            }
        }

        ImGui::End();
        
        // --- END UI FRAME ---
    }

    return 0;
}