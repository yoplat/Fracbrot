#include "app/Benchmark.h"

#include <algorithm>
#include <cstdio>
#include <numeric>

namespace fb {

const std::vector<BenchScene>& benchScenes() {
    static const std::vector<BenchScene> scenes = {
        {.name = "2d_overview", .mode = 0, .view = "-0.5,0,1.2", .iter = 256},
        {.name = "2d_seahorse", .mode = 0, .view = "-0.7453,0.1127,0.003", .iter = 1000},
        // Mostly period-3 bulb interior at a high limit: every interior pixel runs all iterations.
        {.name = "2d_interior", .mode = 0, .view = "-0.122,0.745,0.12", .iter = 5000},
        {.name = "2d_deep", .mode = 0, .view = "0,1,1e-30", .iter = 2000},
        {.name = "3d_quat", .mode = 1, .formula = 0},
        {.name = "3d_quat_close", .mode = 1, .formula = 0, .orbit = glm::vec3(0.12f, 0.08f, 0.5f),
         .target = glm::vec3(-1.25f, 0.0f, 0.0f)},
        {.name = "3d_julia", .mode = 1, .formula = 1},
        {.name = "3d_bulb", .mode = 1, .formula = 2},
        {.name = "3d_bicomplex", .mode = 1, .formula = 3},
        {.name = "3d_hyper", .mode = 1, .formula = 4},
        {.name = "logistic", .mode = 2},
        {.name = "buddhabrot", .mode = 3},
    };
    return scenes;
}

namespace {

double percentile(std::vector<double> v, double p) {
    std::sort(v.begin(), v.end());
    const size_t i = std::min(v.size() - 1, size_t(p * double(v.size() - 1) + 0.5));
    return v[i];
}

} // namespace

bool Benchmark::record(double cpuMs, double totalMs) {
    if (finished()) return false;
    if (m_frame == 0) m_first = totalMs;
    if (m_frame >= kWarmupFrames) {
        m_total.push_back(totalMs);
        m_cpu.push_back(cpuMs);
    }
    if (++m_frame < kWarmupFrames + kMeasuredFrames) return false;

    BenchResult r;
    r.name = benchScenes()[m_scene].name;
    r.firstMs = m_first;
    r.medianMs = percentile(m_total, 0.5);
    r.p95Ms = percentile(m_total, 0.95);
    r.minMs = *std::min_element(m_total.begin(), m_total.end());
    r.meanMs = std::accumulate(m_total.begin(), m_total.end(), 0.0) / double(m_total.size());
    r.cpuMedianMs = percentile(m_cpu, 0.5);
    m_results.push_back(r);

    m_total.clear();
    m_cpu.clear();
    m_frame = 0;
    ++m_scene;
    return true;
}

void Benchmark::report(const std::string& device, uint32_t width, uint32_t height) const {
    std::printf("\n%-16s %9s %9s %9s %9s %9s\n", "scene", "median", "p95", "min", "cpu", "first");
    for (const BenchResult& r : m_results) {
        std::printf("%-16s %7.2fms %7.2fms %7.2fms %7.2fms %7.1fms\n", r.name.c_str(), r.medianMs, r.p95Ms,
                    r.minMs, r.cpuMedianMs, r.firstMs);
    }
    if (m_outPath.empty()) return;

    FILE* f = std::fopen(m_outPath.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "bench: cannot write %s\n", m_outPath.c_str());
        return;
    }
    std::fprintf(f, "{\n  \"device\": \"%s\",\n  \"resolution\": \"%ux%u\",\n  \"scenes\": [\n", device.c_str(), width,
                 height);
    for (size_t i = 0; i < m_results.size(); ++i) {
        const BenchResult& r = m_results[i];
        std::fprintf(f,
                     "    {\"name\": \"%s\", \"median_ms\": %.4f, \"p95_ms\": %.4f, \"min_ms\": %.4f, "
                     "\"mean_ms\": %.4f, \"cpu_median_ms\": %.4f, \"first_ms\": %.4f}%s\n",
                     r.name.c_str(), r.medianMs, r.p95Ms, r.minMs, r.meanMs, r.cpuMedianMs, r.firstMs,
                     i + 1 < m_results.size() ? "," : "");
    }
    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);
    std::printf("wrote %s\n", m_outPath.c_str());
}

} // namespace fb
