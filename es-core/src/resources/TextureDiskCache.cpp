#include "TextureDiskCache.h"

#ifndef _WIN32
#include "resources/TextureResource.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/ZipFile.h"
#include "utils/lz4.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <mutex>
#include <new>
#include <sys/file.h>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
	constexpr uint32_t Magic = 0x32585445;
	constexpr uint32_t MaxBytes = 64 * 1024 * 1024;
	constexpr int CompressionAcceleration = 8;

	struct File
	{
		int fd;

		explicit File(int value) : fd(value) {}

		~File()
		{
			if (fd >= 0)
				::close(fd);
		}

		bool close()
		{
			int value = fd;
			fd = -1;
			return ::close(value) == 0;
		}

		File(const File&) = delete;
		File& operator=(const File&) = delete;
	};

	struct Header
	{
		uint64_t key;
		uint32_t magic, width, height, originalWidth, originalHeight;
		uint32_t raw, compressed, checksum;
	};

	static_assert(sizeof(Header) == 40, "Unexpected cache header layout");

	struct Entry
	{
		std::string path;
		uint64_t bytes;
		timespec modified;
	};

	struct State
	{
		std::atomic<bool> enabled{false}, quit{false};
		std::atomic<uint64_t> limit{0}, retryAfter{0};

		std::string root;
		std::mutex writer, wake;
		std::condition_variable changed;
		std::thread worker;

		bool requested = false;
		bool counted = false;
		uint64_t used = 0;

		~State()
		{
			enabled = false;

			{
				std::lock_guard<std::mutex> lock(wake);
				quit = true;
			}

			changed.notify_one();

			if (worker.joinable())
				worker.join();
		}
	};

	State& state()
	{
		static State value;
		return value;
	}

	bool isHex(const std::string& text)
	{
		return !text.empty() && text.find_first_not_of("0123456789abcdef") == std::string::npos;
	}

	std::string filename(uint64_t key)
	{
		char hex[17];
		std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)key);
		return state().root + "/" + std::string(hex, 2) + "/" + hex + ".cache";
	}

	uint64_t fileSize(const struct stat& st)
	{
		return static_cast<uint64_t>(st.st_size);
	}

	bool transfer(int fd, void* buffer, size_t size, bool writing)
	{
		auto* bytes = static_cast<unsigned char*>(buffer);

		while (size)
		{
			ssize_t n = writing ? ::write(fd, bytes, size) : ::read(fd, bytes, size);

			if (n < 0 && errno == EINTR)
				continue;

			if (n <= 0)
				return false;

			bytes += n;
			size -= static_cast<size_t>(n);
		}

		return true;
	}

	uint32_t checksum(Header header, const char* data)
	{
		header.checksum = 0;
		auto crc = Utils::Zip::ZipFile::computeCRC(0, &header, sizeof(header));
		return Utils::Zip::ZipFile::computeCRC(crc, data, header.compressed);
	}

	uint64_t now()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void requestCleanup()
	{
		auto& s = state();

		if (!s.enabled || now() < s.retryAfter)
			return;

		{
			std::lock_guard<std::mutex> lock(s.wake);

			if (s.requested)
				return;

			s.requested = true;
		}

		s.changed.notify_one();
	}

	bool waitForTextureIdle()
	{
		auto& s = state();

		while (!s.quit && s.enabled)
		{
			if (TextureResource::isLoadingIdle())
				return true;

			std::unique_lock<std::mutex> lock(s.wake);
			s.changed.wait_for(lock, std::chrono::seconds(5), [&]
			{
				return s.quit || !s.enabled;
			});
		}

		return false;
	}

	bool scan(std::vector<Entry>& entries, uint64_t& used)
	{
		auto& s = state();
		std::unique_ptr<DIR, int(*)(DIR*)> top(opendir(s.root.c_str()), closedir);

		if (!top)
			return false;

		for (;;)
		{
			if (!waitForTextureIdle())
				return false;

			errno = 0;
			auto* dir = readdir(top.get());

			if (!dir)
				return errno == 0;

			std::string name = dir->d_name;

			if (name.size() != 2 || !isHex(name))
				continue;

			std::string folder = s.root + "/" + name;
			struct stat st{};

			if (lstat(folder.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
				return false;

			std::unique_ptr<DIR, int(*)(DIR*)> sub(opendir(folder.c_str()), closedir);

			if (!sub)
				return false;

			for (;;)
			{
				if (!waitForTextureIdle())
					return false;

				errno = 0;
				auto* file = readdir(sub.get());

				if (!file)
				{
					if (errno)
						return false;

					break;
				}

				std::string leaf = file->d_name;
				bool temporary = leaf.size() == 33 && leaf.substr(16, 11) == ".cache.tmp.";
				bool cache = leaf.size() == 22 && leaf.substr(16) == ".cache";

				if ((!temporary && !cache) || !isHex(leaf.substr(0, 16)))
					continue;

				std::string path = folder + "/" + leaf;

				if (lstat(path.c_str(), &st) != 0)
				{
					if (errno == ENOENT)
						continue;

					return false;
				}

				if (!S_ISREG(st.st_mode))
					continue;

				if (temporary)
				{
					if (unlink(path.c_str()) != 0 && errno != ENOENT)
						return false;

					continue;
				}

				uint64_t bytes = fileSize(st);
				used += bytes;
				entries.push_back({path, bytes, st.st_mtim});
			}
		}
	}

	void maintain()
	{
		auto& s = state();
		File owner(-1);

		for (;;)
		{
			{
				std::unique_lock<std::mutex> lock(s.wake);
				s.changed.wait(lock, [&] { return s.quit || s.requested; });

				if (s.quit)
					return;

				s.requested = false;
			}

			if (!waitForTextureIdle())
				continue;

			bool ok = false;

			{
				std::lock_guard<std::mutex> lock(s.writer);

				{
					std::lock_guard<std::mutex> wakeLock(s.wake);
					s.requested = false;
				}

				s.counted = false;

				try
				{
					// Only one ES process may change the cache at a time.
					if (owner.fd < 0 && Utils::FileSystem::createDirectory(s.root))
					{
						owner.fd = open((s.root + "/.lock").c_str(),
							O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);

						if (owner.fd >= 0 && flock(owner.fd, LOCK_EX | LOCK_NB) != 0)
							owner.close();
					}

					std::vector<Entry> entries;
					uint64_t used = 0;
					ok = owner.fd >= 0 && scan(entries, used);

					if (ok && used > s.limit)
					{
						std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
						{
							return a.modified.tv_sec != b.modified.tv_sec ?
								a.modified.tv_sec < b.modified.tv_sec :
								a.modified.tv_nsec < b.modified.tv_nsec;
						});

						for (const auto& entry : entries)
						{
							if (used <= s.limit * 4 / 5 || !waitForTextureIdle())
								break;

							if (unlink(entry.path.c_str()) == 0 || errno == ENOENT)
								used -= entry.bytes;
						}
					}

					s.used = used;
					s.counted = ok;
				}
				catch (const std::exception&)
				{
					ok = false;
				}

				if (!ok || s.used > s.limit)
					s.retryAfter = now() + 5000000;
			}
		}
	}
}

namespace TextureDiskCache
{
	void configure(bool value, int sizeMB)
	{
		auto& s = state();
		s.limit = static_cast<uint64_t>(std::max(500, std::min(5000, sizeMB))) * 1000000;

		std::lock_guard<std::mutex> lock(s.wake);

		if (s.root.empty())
			s.root = Paths::getHomePath() + "/.cache/es_texture_cache";

		s.enabled = value;

		if (!value)
		{
			s.changed.notify_one();
			return;
		}

		try
		{
			if (!s.worker.joinable())
				s.worker = std::thread(maintain);
		}
		catch (const std::system_error&)
		{
			s.enabled = false;
			return;
		}

		s.retryAfter = 0;
		s.requested = true;
		s.changed.notify_one();
	}

	bool enabled()
	{
		return state().enabled.load();
	}

	uint64_t makeKey(const std::string& path, float width, float height, bool zoom, int sw, int sh)
	{
		struct stat st{};

		if (path.empty() || path[0] != '/' || stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
			return 0;

		uint64_t hash = 14695981039346656037ULL;

		auto add = [&](const void* data, size_t size)
		{
			const auto* p = static_cast<const unsigned char*>(data);

			while (size--)
			{
				hash ^= *p++;
				hash *= 1099511628211ULL;
			}
		};

		add(path.c_str(), path.size() + 1);

		uint64_t values[] =
		{
			static_cast<uint64_t>(st.st_size),
			static_cast<uint64_t>(st.st_mtim.tv_sec),
			static_cast<uint64_t>(st.st_mtim.tv_nsec),
			static_cast<uint64_t>(st.st_ctim.tv_sec),
			static_cast<uint64_t>(st.st_ctim.tv_nsec),
			static_cast<uint64_t>(zoom),
			static_cast<uint64_t>(sw),
			static_cast<uint64_t>(sh)
		};

		add(values, sizeof(values));
		add(&width, sizeof(width));
		add(&height, sizeof(height));

		return hash ? hash : 1;
	}

	bool read(uint64_t key, Image& image) noexcept
	{
		try
		{
			if (!key || !enabled())
				return false;

			File file(open(filename(key).c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
			Header h{};
			struct stat st{};

			if (file.fd < 0 || fstat(file.fd, &st) != 0 ||
				!S_ISREG(st.st_mode) || !transfer(file.fd, &h, sizeof(h), false))
			{
				return false;
			}

			if (h.magic != Magic || h.key != key ||
				!h.width || !h.height || h.width > 16384 || h.height > 16384 ||
				!h.originalWidth || !h.originalHeight ||
				h.originalWidth > INT32_MAX || h.originalHeight > INT32_MAX ||
				static_cast<uint64_t>(h.width) * h.height * 4 != h.raw ||
				h.raw > MaxBytes || !h.compressed ||
				h.compressed > static_cast<uint32_t>(LZ4_compressBound(h.raw)) ||
				st.st_size != static_cast<int64_t>(sizeof(h) + h.compressed))
			{
				return false;
			}

			std::unique_ptr<char[]> packed(new (std::nothrow) char[h.compressed]);

			if (!packed || !transfer(file.fd, packed.get(), h.compressed, false) ||
				checksum(h, packed.get()) != h.checksum)
			{
				return false;
			}

			std::unique_ptr<unsigned char[]> pixels(new (std::nothrow) unsigned char[h.raw]);

			if (!pixels)
				return false;

			int size = LZ4_decompress_safe(
				packed.get(), reinterpret_cast<char*>(pixels.get()), h.compressed, h.raw);

			if (size != static_cast<int>(h.raw))
				return false;

			image.pixels = std::move(pixels);
			image.width = h.width;
			image.height = h.height;
			image.originalWidth = h.originalWidth;
			image.originalHeight = h.originalHeight;

			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	bool write(uint64_t key, const Image& image) noexcept
	{
		try
		{
			auto& s = state();

			if (!key || !s.enabled || !image.pixels ||
				!image.width || !image.height ||
				image.width > 16384 || image.height > 16384 ||
				!image.originalWidth || !image.originalHeight ||
				image.originalWidth > INT32_MAX || image.originalHeight > INT32_MAX ||
				now() < s.retryAfter)
			{
				return false;
			}

			uint64_t raw = static_cast<uint64_t>(image.width) * image.height * 4;

			if (raw > MaxBytes)
				return false;

			// Skip this write if the cache is busy.
			std::unique_lock<std::mutex> lock(s.writer, std::try_to_lock);

			if (!lock.owns_lock())
				return false;

			if (!s.counted || s.used > s.limit)
			{
				requestCleanup();
				return false;
			}

			int bound = LZ4_compressBound(static_cast<int>(raw));
			std::unique_ptr<char[]> packed(new (std::nothrow) char[bound]);

			if (!packed)
				return false;

			int size = LZ4_compress_fast(reinterpret_cast<const char*>(image.pixels.get()), packed.get(), static_cast<int>(raw), bound, CompressionAcceleration);

			if (size <= 0)
				return false;

			Header h
			{
				key, Magic, image.width, image.height, image.originalWidth, image.originalHeight,
				static_cast<uint32_t>(raw), static_cast<uint32_t>(size), 0
			};

			h.checksum = checksum(h, packed.get());

			std::string path = filename(key);
			std::string temporary = path + ".tmp.XXXXXX";
			std::vector<char> name(temporary.begin(), temporary.end());
			name.push_back('\0');

			File file(mkostemp(name.data(), O_CLOEXEC));

			if (file.fd < 0 && errno == ENOENT)
			{
				if (Utils::FileSystem::createDirectory(path.substr(0, path.rfind('/'))))
				{
					std::copy(temporary.begin(), temporary.end(), name.begin());
					file.fd = mkostemp(name.data(), O_CLOEXEC);
				}
			}

			if (file.fd < 0)
			{
				s.retryAfter = now() + 5000000;
				return false;
			}

			bool ok = transfer(file.fd, &h, sizeof(h), true) && transfer(file.fd, packed.get(), size, true);

			if (!file.close())
				ok = false;

			uint64_t old = 0;

			if (ok && s.enabled)
			{
				struct stat previous{};

				if (lstat(path.c_str(), &previous) == 0)
				{
					if (S_ISREG(previous.st_mode))
						old = fileSize(previous);
					else
						ok = false;
				}
				else if (errno != ENOENT)
					ok = false;
			}
			else
				ok = false;

			if (ok)
				ok = rename(name.data(), path.c_str()) == 0;

			if (ok)
			{
				uint64_t bytes = sizeof(h) + static_cast<uint64_t>(size);
				s.used = s.used >= old ? s.used - old + bytes : bytes;

				if (s.used > s.limit)
					requestCleanup();
			}
			else
			{
				if (unlink(name.data()) != 0 && errno != ENOENT)
					s.counted = false;

				s.retryAfter = now() + 5000000;
			}

			return ok;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}
}

#else

namespace TextureDiskCache
{
	void configure(bool, int) {}
	bool enabled() { return false; }

	uint64_t makeKey(const std::string&, float, float, bool, int, int) { return 0; }
	bool read(uint64_t, Image&) noexcept { return false; }
	bool write(uint64_t, const Image&) noexcept { return false; }
}

#endif
