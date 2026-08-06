#include "renderer-benchmark-window.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace
{
std::uint64_t peakResidentBytes()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX counters {};
	if (!GetProcessMemoryInfo(GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters)))
		return 0;
	return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
	rusage usage {};
	if (getrusage(RUSAGE_SELF, &usage) != 0)
		return 0;
#if defined(__APPLE__)
	return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
	return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
#endif
}

double percentile95(std::vector<double> samples)
{
	std::sort(samples.begin(), samples.end());
	const std::size_t index = std::min(samples.size() - 1,
		(samples.size() * 95 + 99) / 100 - 1);
	return samples[index];
}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		std::cerr << "Usage: slint-renderer-benchmark <report.json> <renderer> [cycles]\n";
		return 2;
	}
	const std::filesystem::path reportPath = argv[1];
	const std::string renderer = argv[2];
	const unsigned measuredCycles = argc > 3 ? static_cast<unsigned>(std::stoul(argv[3])) : 120U;
	if (measuredCycles == 0)
	{
		std::cerr << "Cycle count must be positive\n";
		return 2;
	}

	auto window = RendererBenchmarkWindow::create();
	window->set_benchmark_width(1280.0f);
	window->set_benchmark_height(760.0f);
	window->window().set_size(slint::PhysicalSize { slint::Size<std::uint32_t> { 1280, 760 } });

	constexpr unsigned warmupCycles = 12;
	std::vector<double> cycleTimes;
	cycleTimes.reserve(measuredCycles);
	unsigned completedCycles = 0;
	bool timedOut = false;
	auto previousCycle = std::chrono::steady_clock::now();
	slint::Timer redrawTimer;
	unsigned requestedCycle = 0;
	redrawTimer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(16), [&]
	{
		const auto now = std::chrono::steady_clock::now();
		if (completedCycles >= warmupCycles)
			cycleTimes.push_back(std::chrono::duration<double, std::milli>(now - previousCycle).count());
		previousCycle = now;
		window->set_benchmark_theme(static_cast<Theme>(requestedCycle % 5));
		window->set_benchmark_tab(requestedCycle % 2 == 0 ? AppTab::Torrents : AppTab::Logs);
		window->window().request_redraw();
		++requestedCycle;
		++completedCycles;
		if (completedCycles >= warmupCycles + measuredCycles)
			slint::quit_event_loop();
	});
	slint::Timer timeoutTimer;
	timeoutTimer.start(slint::TimerMode::SingleShot, std::chrono::seconds(30), [&]
	{
		timedOut = true;
		slint::quit_event_loop();
	});
	window->show();
	const std::clock_t cpuStart = std::clock();
	const auto wallStart = std::chrono::steady_clock::now();
	slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
	const auto wallEnd = std::chrono::steady_clock::now();
	const std::clock_t cpuEnd = std::clock();
	redrawTimer.stop();
	timeoutTimer.stop();
	window->hide();
	if (timedOut || cycleTimes.size() != measuredCycles)
	{
		std::cerr << "Renderer completed " << cycleTimes.size() << '/' << measuredCycles
			<< " measured cycles before the stability timeout\n";
		return 1;
	}

	const double wallMs = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();
	const double cpuMs = 1000.0 * static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;
	const double totalCycleMs = std::accumulate(cycleTimes.begin(), cycleTimes.end(), 0.0);
	std::error_code error;
	std::filesystem::create_directories(reportPath.parent_path(), error);
	std::ofstream report(reportPath);
	if (!report)
	{
		std::cerr << "Cannot write benchmark report: " << reportPath << '\n';
		return 1;
	}
	report << "{\n"
		<< "  \"renderer\": \"" << renderer << "\",\n"
		<< "  \"warmup_cycles\": " << warmupCycles << ",\n"
		<< "  \"measured_cycles\": " << measuredCycles << ",\n"
		<< "  \"completed_cycles\": " << completedCycles << ",\n"
		<< "  \"wall_ms\": " << wallMs << ",\n"
		<< "  \"cpu_ms\": " << cpuMs << ",\n"
		<< "  \"mean_cycle_ms\": " << totalCycleMs / measuredCycles << ",\n"
		<< "  \"p95_cycle_ms\": " << percentile95(cycleTimes) << ",\n"
		<< "  \"peak_rss_bytes\": " << peakResidentBytes() << ",\n"
		<< "  \"stable\": true\n"
		<< "}\n";
	std::cout << renderer << ": " << measuredCycles << " redraw cycles, "
		<< totalCycleMs / measuredCycles << " ms/cycle mean, " << cpuMs
		<< " ms CPU, " << peakResidentBytes() / (1024 * 1024) << " MiB peak RSS\n";
	return 0;
}
