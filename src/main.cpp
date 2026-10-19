#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ble.hpp"
#include "data_queue.hpp"
#include "logger.hpp"
#include "plot_buffer.hpp"
#include "csv_loader.hpp"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static DataQueue                                         g_queue;
static std::unordered_map<std::string, DevicePlotBuffer> g_plotBuffers;

// ---------------------------------------------------------------------------
// Frame update: drain queue and feed plot buffers
// ---------------------------------------------------------------------------
static void drainQueue()
{
    thread_local std::vector<std::unique_ptr<Packet>> batch;
    batch.clear();
    g_queue.drainInto(batch);

    for (auto& pkt : batch) {
        const std::string& addr = pkt->deviceAddress;
        if (addr.empty())
            continue;

        auto it = g_plotBuffers.find(addr);
        if (it == g_plotBuffers.end()) {
            LOG_INFO("New device seen: {} ({})", pkt->deviceType(), addr);
            it = g_plotBuffers.emplace(
                    addr,
                    DevicePlotBuffer(addr, std::string(pkt->deviceType())))
                .first;
        }
        it->second.update(*pkt);
    }
}

// ---------------------------------------------------------------------------
// UI — connection panel
// ---------------------------------------------------------------------------
static void renderConnectionPanel(BLEManager& ble)
{
    ImGui::Begin("BLE Connection");

    if (!ble.adapterAvailable()) {
        ImGui::TextColored({1, 0.3f, 0.3f, 1}, "No Bluetooth adapter found");
    } else if (ble.isScanning()) {
        ImGui::TextColored({0.3f, 1, 0.3f, 1}, "Scanning...");
        if (ImGui::Button("Stop Scan"))
            ble.stopScan();
    } else {
        if (ImGui::Button("Start Scan"))
            ble.startScan();
    }

    ImGui::Separator();
    ImGui::Text("Connected devices:");
    for (const auto& addr : ble.connectedDevices())
        ImGui::BulletText("%s", addr.c_str());

        ImGui::Separator();
    static char        csvPath[512] = "ble_log.csv";
    static std::string csvError;

    ImGui::SetNextItemWidth(-90);
    ImGui::InputText("##csvpath", csvPath, sizeof(csvPath));
    ImGui::SameLine();
    if (ImGui::Button("Load CSV")) {
        if (auto buf = loadPlotFromCsv(csvPath, csvError)) {
            std::string key = buf->address();
            g_plotBuffers.insert_or_assign(key, std::move(*buf)); // reload replaces
            csvError.clear();
        } else {
            LOG_ERROR("CSV load failed: {}", csvError);
        }
    }
    if (!csvError.empty())
        ImGui::TextColored({1, 0.3f, 0.3f, 1}, "%s", csvError.c_str());

    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI — plot panel for one device
// ---------------------------------------------------------------------------
static void renderDevicePlot(DevicePlotBuffer& buf)
{
    std::string title = buf.deviceType() + "  " + buf.address();
    ImGui::Begin(title.c_str());

    // Channel visibility toggles
    for (const auto& name : buf.channelNames()) {
        ChannelBuffer* ch = buf.channel(name);
        if (!ch) continue;
        ImGui::SameLine();
        ImGui::Checkbox(name.c_str(), &ch->visible);
    }

    if (buf.consumeFitRequest())
        ImPlot::SetNextAxesToFit();

    if (ImPlot::BeginPlot("##sensor", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("time (s)", nullptr);
        if (!buf.isStatic()) {
            double t = buf.latestTime();
            ImPlot::SetupAxisLimits(ImAxis_X1, t - 10.0, t + 0.5, ImGuiCond_Always);
        }
        // ... channel plotting unchanged

        for (const auto& name : buf.channelNames()) {
            const ChannelBuffer* ch = buf.channel(name);
            if (!ch || !ch->visible)
                continue;
            ImPlot::PlotLineG(ch->name.c_str(), ChannelBuffer::getter,
                              const_cast<void*>(static_cast<const void*>(ch)),
                              ch->plotCount());
        }
        ImPlot::EndPlot();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI — debug log window (compiled only when ENABLE_LOGGING is defined)
// ---------------------------------------------------------------------------
#ifdef ENABLE_LOGGING
static void renderDebugLog()
{
    ImGui::Begin("Debug Log");

    // Level filter checkboxes
    static bool showDebug = true, showInfo = true, showWarn = true, showError = true;
    ImGui::Checkbox("DBG", &showDebug); ImGui::SameLine();
    ImGui::Checkbox("INF", &showInfo);  ImGui::SameLine();
    ImGui::Checkbox("WRN", &showWarn);  ImGui::SameLine();
    ImGui::Checkbox("ERR", &showError); ImGui::SameLine();
    if (ImGui::Button("Clear"))
        Logger::instance().clear();

    ImGui::Separator();

    ImGui::BeginChild("log_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    auto entries = Logger::instance().snapshot();
    for (const auto& e : entries) {
        if (e.level == LogLevel::Debug && !showDebug) continue;
        if (e.level == LogLevel::Info  && !showInfo)  continue;
        if (e.level == LogLevel::Warn  && !showWarn)  continue;
        if (e.level == LogLevel::Error && !showError) continue;

        ImVec4 colour;
        const char* prefix;
        switch (e.level) {
        case LogLevel::Debug: colour = {0.6f, 0.6f, 0.6f, 1}; prefix = "[DBG]"; break;
        case LogLevel::Info:  colour = {1,    1,    1,    1}; prefix = "[INF]"; break;
        case LogLevel::Warn:  colour = {1,    0.8f, 0.2f, 1}; prefix = "[WRN]"; break;
        case LogLevel::Error: colour = {1,    0.3f, 0.3f, 1}; prefix = "[ERR]"; break;
        }

        ImGui::TextColored(colour, "%s %-20s %s",
                           prefix, e.tag.c_str(), e.msg.c_str());
    }

    // Auto-scroll to bottom when new entries arrive
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}
#endif

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    LOG_INFO("Application starting");

    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "BLE Plotter", nullptr, nullptr);
    if (!window) {
        LOG_ERROR("glfwCreateWindow failed");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    LOG_INFO("ImGui + ImPlot initialised");

    BLEManager ble(&g_queue);
    ble.startScan();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        drainQueue();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderConnectionPanel(ble);

        for (auto& [addr, buf] : g_plotBuffers)
            renderDevicePlot(buf);

#ifdef ENABLE_LOGGING
        renderDebugLog();
#endif

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    LOG_INFO("Shutting down");
    ble.disconnectAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
