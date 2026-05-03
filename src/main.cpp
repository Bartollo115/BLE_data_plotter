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
#include "plot_buffer.hpp"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static DataQueue                                        g_queue;
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

    if (ImPlot::BeginPlot("##sensor", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("time (s)", nullptr);
        ImPlot::SetupAxisLimits(ImAxis_X1,
            ImGui::GetTime() - 10.0, ImGui::GetTime(), ImGuiCond_Always);

        for (const auto& name : buf.channelNames()) {
            const ChannelBuffer* ch = buf.channel(name);
            if (!ch || !ch->visible)
                continue;
            ImPlot::PlotLine(ch->name.c_str(),
                             ch->xData(), ch->yData(),
                             ch->plotCount(), 0, ch->plotOffset(),
                             sizeof(float));
        }
        ImPlot::EndPlot();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "BLE Plotter", nullptr, nullptr);
    if (!window) {
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

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ble.disconnectAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
