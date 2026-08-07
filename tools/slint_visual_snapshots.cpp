#include "snapshot-window.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct View
{
	const char *name;
	AppTab tab;
	bool addDialog = false;
	bool removeDialog = false;
	bool emptyTorrents = false;
	bool largeTorrentModel = false;
	bool emptySearch = false;
	bool emptyFavorites = false;
	bool emptyLogs = false;
	bool emptyDetails = false;
};

std::shared_ptr<slint::Model<TorrentRow>> makeLargeTorrentModel()
{
	std::vector<TorrentRow> rows;
	rows.reserve(10000);
	for (int index = 0; index < 10000; ++index)
	{
		char suffix[5] {};
		std::snprintf(suffix, sizeof(suffix), "%04x", index);
		TorrentRow row;
		row.id = slint::SharedString("v1:" + std::string(36, '0') + suffix);
		row.name = slint::SharedString("Large model torrent " + std::to_string(index + 1) + " with a long elided title");
		row.state_label = slint::SharedString(index % 3 == 0 ? "Downloading" : "Seeding");
		row.progress = static_cast<float>(index % 101) / 100.0f;
		row.progress_label = slint::SharedString(std::to_string(index % 101) + "%");
		row.size_label = slint::SharedString("1.0 GiB");
		row.download_rate_label = slint::SharedString(index % 3 == 0 ? "2.4 MiB/s" : "0 B/s");
		row.upload_rate_label = slint::SharedString("128 KiB/s");
		row.peers_label = slint::SharedString("12");
		row.seeds_label = slint::SharedString("48");
		row.eta_label = slint::SharedString("8m");
		row.active = true;
		rows.push_back(std::move(row));
	}
	return std::make_shared<slint::VectorModel<TorrentRow>>(std::move(rows));
}

void writeU16(std::ofstream &output, std::uint16_t value)
{
	output.put(static_cast<char>(value & 0xff));
	output.put(static_cast<char>((value >> 8) & 0xff));
}

void writeU32(std::ofstream &output, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		output.put(static_cast<char>((value >> shift) & 0xff));
}

bool writeBmp(const std::filesystem::path &path, const slint::SharedPixelBuffer<slint::Rgba8Pixel> &pixels)
{
	std::ofstream output(path, std::ios::binary);
	if (!output)
		return false;
	const std::uint32_t rowSize = (pixels.width() * 3u + 3u) & ~3u;
	const std::uint32_t pixelBytes = rowSize * pixels.height();
	output.put('B'); output.put('M');
	writeU32(output, 54u + pixelBytes);
	writeU32(output, 0); writeU32(output, 54);
	writeU32(output, 40); writeU32(output, pixels.width()); writeU32(output, pixels.height());
	writeU16(output, 1); writeU16(output, 24); writeU32(output, 0); writeU32(output, pixelBytes);
	writeU32(output, 2835); writeU32(output, 2835); writeU32(output, 0); writeU32(output, 0);
	const std::array<char, 3> padding {};
	for (std::uint32_t y = pixels.height(); y-- > 0;)
	{
		const auto *row = pixels.begin() + y * pixels.width();
		for (std::uint32_t x = 0; x < pixels.width(); ++x)
		{
			output.put(static_cast<char>(row[x].b));
			output.put(static_cast<char>(row[x].g));
			output.put(static_cast<char>(row[x].r));
		}
		output.write(padding.data(), rowSize - pixels.width() * 3u);
	}
	return output.good();
}
}

int main(int argc, char **argv)
{
	const std::filesystem::path outputDirectory = argc > 1 ? argv[1] : "slint-snapshots";
	const std::string requestedView = argc > 2 ? argv[2] : "";
	const std::string requestedSize = argc > 3 ? argv[3] : "";
	std::error_code error;
	std::filesystem::create_directories(outputDirectory, error);
	if (error)
	{
		std::cerr << "Cannot create snapshot directory: " << error.message() << '\n';
		return 1;
	}

	const std::vector<std::pair<const char *, Theme>> themes {
		{ "dark", Theme::Dark }, { "ocean", Theme::Ocean }, { "nord", Theme::Nord },
		{ "dracula", Theme::Dracula }, { "cyberpunk", Theme::Cyberpunk }
	};
	const std::vector<std::pair<unsigned, unsigned>> sizes {
		{ 800, 600 }, { 900, 700 }, { 1024, 768 }, { 1280, 760 }, { 1440, 900 }, { 1920, 1080 }
	};
	const std::vector<View> views {
		{ .name = "torrents", .tab = AppTab::Torrents }, { .name = "search", .tab = AppTab::Search },
		{ .name = "favorites", .tab = AppTab::Favorites }, { .name = "logs", .tab = AppTab::Logs },
		{ .name = "preferences", .tab = AppTab::Preferences },
		{ .name = "add-dialog", .tab = AppTab::Torrents, .addDialog = true },
		{ .name = "remove-dialog", .tab = AppTab::Torrents, .removeDialog = true },
		{ .name = "torrents-empty", .tab = AppTab::Torrents, .emptyTorrents = true },
		{ .name = "torrents-10000", .tab = AppTab::Torrents, .largeTorrentModel = true },
		{ .name = "search-empty", .tab = AppTab::Search, .emptySearch = true },
		{ .name = "favorites-empty", .tab = AppTab::Favorites, .emptyFavorites = true },
		{ .name = "logs-empty", .tab = AppTab::Logs, .emptyLogs = true },
		{ .name = "details-empty", .tab = AppTab::Torrents, .emptyDetails = true }
	};
	const auto largeTorrentModel = makeLargeTorrentModel();

	std::size_t generated = 0;
	for (const auto &[width, height] : sizes)
	{
		const std::string sizeName = std::to_string(width) + "x" + std::to_string(height);
		if (!requestedSize.empty() && requestedSize != sizeName)
			continue;
		for (const auto &[themeName, theme] : themes)
		{
			for (const auto &view : views)
			{
				if (!requestedView.empty() && requestedView != view.name)
					continue;
				auto window = SnapshotWindow::create();
				window->set_snapshot_tab(view.tab);
				window->set_snapshot_theme(theme);
				window->set_snapshot_add_dialog(view.addDialog);
				window->set_snapshot_remove_dialog(view.removeDialog);
				window->set_snapshot_empty_torrents(view.emptyTorrents);
				window->set_snapshot_empty_search(view.emptySearch);
				window->set_snapshot_empty_favorites(view.emptyFavorites);
				window->set_snapshot_empty_logs(view.emptyLogs);
				window->set_snapshot_empty_details(view.emptyDetails);
				window->set_snapshot_width(static_cast<float>(width));
				window->set_snapshot_height(static_cast<float>(height));
				if (view.largeTorrentModel)
					window->set_snapshot_torrent_rows(largeTorrentModel);
				window->window().set_size(slint::PhysicalSize { slint::Size<uint32_t> { width, height } });
				window->show();
				const auto pixels = window->window().take_snapshot();
				window->hide();
				if (!pixels)
				{
					std::cerr << "Renderer could not capture " << view.name << " at " << width << 'x' << height << '\n';
					return 1;
				}
				const auto filename = std::string(view.name) + '-' + themeName + '-' + std::to_string(width) + 'x' + std::to_string(height) + ".bmp";
				if (!writeBmp(outputDirectory / filename, *pixels))
				{
					std::cerr << "Cannot write " << filename << '\n';
					return 1;
				}
				++generated;
			}
		}
	}
	if (generated == 0)
	{
		std::cerr << "No snapshot matched the requested filters\n";
		return 1;
	}
	std::cout << "Generated " << generated << " Slint snapshots in " << outputDirectory << '\n';
	return 0;
}
