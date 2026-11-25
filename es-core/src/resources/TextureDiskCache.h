#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace TextureDiskCache
{
	struct Image
	{
		std::unique_ptr<unsigned char[]> pixels;
		uint32_t width = 0, height = 0;
		uint32_t originalWidth = 0, originalHeight = 0;
	};

	void configure(bool enabled, int sizeMB);
	bool enabled();

	uint64_t makeKey(const std::string& path, float width, float height, bool zoom, int screenWidth, int screenHeight);
	bool read(uint64_t key, Image& image) noexcept;
	bool write(uint64_t key, const Image& image) noexcept;
}
