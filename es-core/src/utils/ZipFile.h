#pragma once

#include <string>
#include <vector>
#include <map>

namespace Utils
{
	namespace Zip
	{
		typedef size_t(*zip_callback)(void *pOpaque, unsigned long long file_ofs, const void *pBuf, size_t n);
		
		struct ZipInfo
		{
			std::string filename;
			std::size_t compress_size = 0;
			std::size_t file_size = 0;
			uint32_t crc = 0;
		};

		class ZipFile
		{
		public:
			ZipFile();
			~ZipFile();

			bool load(const std::string &filename);
			bool extract(const std::string &member, const std::string &path, bool pathIsFullPath = false);

			bool readBuffered(const std::string &name, zip_callback pCallback, void* pOpaque);

			std::string getFileCrc(const std::string &name);
			std::string getFileMd5(const std::string &name);

			std::vector<std::string> namelist();
			std::vector<ZipInfo> infolist();

			static unsigned int computeCRC(unsigned int crc, const void* ptr, size_t buf_len);

		private:
			std::string getInternalFilename(const std::string& fileName);

			std::map<std::string, std::string> mUtfTo437Names;
			void* mZipFile;

		}; // DateTime
	}
}
