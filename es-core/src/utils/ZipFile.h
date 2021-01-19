#pragma once

#include <string>
#include <vector>

namespace Utils
{
	namespace Zip
	{
		typedef size_t(*zip_callback)(void *pOpaque, unsigned long long file_ofs, const void *pBuf, size_t n);
		
		class ZipFile
		{
		public:
			ZipFile();
			~ZipFile();

			bool load(const std::string &filename);
			bool extract(const std::string &member, const std::string &path);

			void readBuffered(const std::string &name, zip_callback pCallback, void* pOpaque);

			std::string getFileCrc(const std::string &name);

			std::vector<std::string> namelist();

			static unsigned int computeCRC(unsigned int crc, const void* ptr, size_t buf_len);

		private:
			void* mZipFile;

		}; // DateTime
	}
}
