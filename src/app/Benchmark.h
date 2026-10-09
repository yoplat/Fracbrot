#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fb {

// A fixed view to time. Fields left unset keep the mode's defaults.
struct BenchScene {
    const char* name;
    int mode;                     // ViewMode
    const char* view = nullptr;   // 2D "X,Y,SCALE"
    uint32_t iter = 256;          // 2D / plane iterations
    int formula = 0;              // Formula3D
    std::optional<glm::vec3> orbit;  // yaw, pitch, distance
    std::optional<glm::dvec3> target;
};

const std::vector<BenchScene>& benchScenes();
// Restricts benchScenes() to scenes whose name contains `substring`.
void setBenchFilter(const std::string& substring);

struct BenchResult {
    std::string name;
    double firstMs = 0.0; // first frame incl. one-off work (e.g. deep-zoom reference orbit)
    double medianMs = 0.0, p95Ms = 0.0, minMs = 0.0, meanMs = 0.0;
    double cpuMedianMs = 0.0; // encode + submit only
};

// Drives the scene list frame by frame. Each frame's latency is measured from right after
// the swapchain texture is acquired (so vsync waits are excluded) until the GPU reports the
// submitted work done.
class Benchmark {
public:
    static constexpr int kWarmupFrames = 15;
    static constexpr int kMeasuredFrames = 90;

    explicit Benchmark(std::string outPath) : m_outPath(std::move(outPath)) {}

    size_t sceneIndex() const { return m_scene; }
    bool finished() const { return m_scene >= benchScenes().size(); }
    // Records one frame; returns true when the current scene is complete and the next began.
    bool record(double cpuMs, double totalMs);
    // Prints a table and writes JSON. `device` and `resolution` are informational.
    void report(const std::string& device, uint32_t width, uint32_t height) const;

private:
    std::string m_outPath;
    size_t m_scene = 0;
    int m_frame = 0;
    std::vector<double> m_total, m_cpu;
    std::vector<BenchResult> m_results;
    double m_first = 0.0;
};

} // namespace fb
