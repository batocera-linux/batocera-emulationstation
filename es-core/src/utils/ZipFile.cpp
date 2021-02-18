#include "ZipFile.h"
#include <time.h>
#include "LocaleES.h"

#if WIN32
#include <Windows.h>
#endif

#include <functional>
#include <iterator>
#include <iostream>
#include <cstring>
#include <string>
#include "zip_file.hpp"
#include "FileSystemUtil.h"
#include "md5.h"
#include "Log.h"

namespace Utils
{
	namespace Zip
	{
		unsigned int ZipFile::computeCRC(unsigned int crc, const void* ptr, size_t buf_len)
		{			
			return (mz_uint32)mz_crc32((mz_uint32)crc, (const mz_uint8 *)ptr, buf_len);
		}

		#define mZipArchive   ((mz_zip_archive*) mZipFile)

		static const uint16_t cp437_to_unicode[256] = {
			0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
			0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
			0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
			0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
			0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
			0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
			0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
			0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
			0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
			0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
			0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
			0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
			0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
			0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
			0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
			0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0 };

		#define UTF_8_LEN_2_MASK 0xe0
		#define UTF_8_LEN_2_MATCH 0xc0
		#define UTF_8_LEN_3_MASK 0xf0
		#define UTF_8_LEN_3_MATCH 0xe0
		#define UTF_8_LEN_4_MASK 0xf8
		#define UTF_8_LEN_4_MATCH 0xf0
		#define UTF_8_CONTINUE_MASK 0xc0
		#define UTF_8_CONTINUE_MATCH 0x80
		
		static uint32_t unicode_to_utf8_len(uint32_t codepoint) 
		{
			if (codepoint < 0x0080)
				return 1;
			if (codepoint < 0x0800)
				return 2;
			if (codepoint < 0x10000)
				return 3;

			return 4;
		}

		static uint32_t unicode_to_utf8(uint32_t codepoint, uint8_t *buf) 
		{
			if (codepoint < 0x0080) 
			{
				buf[0] = codepoint & 0xff;
				return 1;
			}
			if (codepoint < 0x0800) 
			{
				buf[0] = (uint8_t)(UTF_8_LEN_2_MATCH | ((codepoint >> 6) & 0x1f));
				buf[1] = (uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
				return 2;
			}
			if (codepoint < 0x10000) 
			{
				buf[0] = (uint8_t)(UTF_8_LEN_3_MATCH | ((codepoint >> 12) & 0x0f));
				buf[1] = (uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 6) & 0x3f));
				buf[2] = (uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
				return 3;
			}
			buf[0] = (uint8_t)(UTF_8_LEN_4_MATCH | ((codepoint >> 18) & 0x07));
			buf[1] = (uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 12) & 0x3f));
			buf[2] = (uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 6) & 0x3f));
			buf[3] = (uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
			return 4;
		}

		std::string cp437_to_utf8(const std::string& _cp437buf)
		{
			int len = _cp437buf.size();
			if (len == 0)
				return "";

			const unsigned char* cp437buf = (const unsigned char*)_cp437buf.c_str();

			uint32_t i;

			int buflen = 1;
			for (i = 0; i < len; i++)
				buflen += unicode_to_utf8_len(cp437_to_unicode[cp437buf[i]]);

			unsigned char* utf8buf = new unsigned char[buflen];

			int offset = 0;
			for (i = 0; i < len; i++)
				offset += unicode_to_utf8(cp437_to_unicode[cp437buf[i]], utf8buf + offset);

			utf8buf[buflen - 1] = 0;			

			std::string ret = (char*)utf8buf;
			delete utf8buf;

			return ret;
		}

		ZipFile::ZipFile()
		{
			mZipFile = nullptr;			
		}

		ZipFile::~ZipFile()
		{
			if (mZipFile != nullptr)
			{
				mz_zip_reader_end(mZipArchive);
				delete mZipArchive;
			}
		}

		bool ZipFile::load(const std::string &filename)
		{
			try
			{
				if (mZipFile != nullptr)
				{
					mz_zip_reader_end(mZipArchive);
					delete mZipArchive;
					mZipFile = nullptr;
				}

				mz_zip_archive* zipArchive = new mz_zip_archive();
				memset(zipArchive, 0, sizeof(mz_zip_archive));
				mz_bool status = mz_zip_reader_init_file(zipArchive, filename.c_str(), 0);
				if (!status)
				{
					delete zipArchive;
					return false;
				}

				mZipFile = zipArchive;
			}
			catch (std::runtime_error& e)
			{
				LOG(LogError) << "ZipFile::load error : " << e.what();
			}
			catch (...)
			{
				LOG(LogError) << "ZipFile::load unknown error";
			}

			return mZipFile != nullptr;
		}

		std::string iso_8859_1_to_utf8(const std::string &str)
		{
			std::string strOut;
			for (auto it = str.cbegin(); it != str.cend(); ++it)
			{
				uint8_t ch = *it;
				if (ch < 0x80)
					strOut.push_back(ch);
				else 
				{
					strOut.push_back(0xc0 | ch >> 6);
					strOut.push_back(0x80 | (ch & 0x3f));
				}
			}
			return strOut;
		}

		std::vector<ZipInfo> ZipFile::infolist()
		{
			std::vector<ZipInfo> ret;

			if (mZipArchive == nullptr)
				return ret;

			mUtfTo437Names.clear();

			for (int i = 0; i < (int)mz_zip_reader_get_num_files(mZipArchive); i++)
			{
				mz_zip_archive_file_stat file_stat;
				if (!mz_zip_reader_file_stat(mZipArchive, i, &file_stat))
					continue;

				ZipInfo zi;

				bool isUtf8 = (file_stat.m_bit_flag & (1 << 11)) ? true : false;
				if (!isUtf8)
					zi.filename = cp437_to_utf8(file_stat.m_filename);
				else
					zi.filename = file_stat.m_filename;
				
				zi.file_size = file_stat.m_uncomp_size;
				zi.compress_size = file_stat.m_comp_size;
				zi.crc = file_stat.m_crc32;				

				ret.push_back(zi);

				if (zi.filename != file_stat.m_filename)
					mUtfTo437Names[zi.filename] = file_stat.m_filename;
			}

			return ret;
		}

		std::vector<std::string> ZipFile::namelist()
		{
			std::vector<std::string> ret;

			for (auto it : infolist())
				ret.push_back(it.filename);

			return ret;
		}

		std::string join_path(const std::vector<std::string> &parts)
		{
			std::string joined;
			std::size_t i = 0;
			for (auto part : parts)
			{
				joined.append(part);

				if (i++ != parts.size() - 1)
					joined.append(1, '/');
			}

			return joined;
		}

		bool ZipFile::extract(const std::string &member, const std::string &path, bool pathIsFullPath)
		{
			if (mZipFile == nullptr)
				return false;

			auto fullPath = join_path({ path, member });
			return mz_zip_reader_extract_file_to_file(mZipArchive, getInternalFilename(member).c_str(), (pathIsFullPath ? path : fullPath).c_str(), 0);
		}

		struct OpaqueWrapper
		{
			zip_callback callback;
			void* opaque;
		};

		std::string ZipFile::getInternalFilename(const std::string& fileName)
		{
			if (mUtfTo437Names.size() == 0 && !fileName.empty())
				infolist();

			auto it = mUtfTo437Names.find(fileName);
			if (it != mUtfTo437Names.cend())
				return it->second;

			return fileName;
		}

		bool ZipFile::readBuffered(const std::string &name, zip_callback pCallback, void* pOpaque)
		{
			if (mZipFile == nullptr)
				return false;
			
			try
			{
				OpaqueWrapper w;
				w.opaque = pOpaque;
				w.callback = pCallback;

				mz_file_write_func func = [](void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n)
				{
					OpaqueWrapper* wrapper = (OpaqueWrapper*)pOpaque;
					return wrapper->callback(wrapper->opaque, file_ofs, pBuf, n);
				};

				return mz_zip_reader_extract_file_to_callback(mZipArchive, getInternalFilename(name).c_str(), pCallback, pOpaque, 0);
			}
			catch (...)
			{

			}

			return false;
		}

		std::string ZipFile::getFileCrc(const std::string &name)
		{
			if (mZipFile == nullptr)
				return "";

			try
			{
				for (auto it : infolist())
				{
					if (it.filename != name)
						continue;

					char hex[10];
					auto len = snprintf(hex, sizeof(hex) - 1, "%08X", it.crc);
					hex[len] = 0;
					return hex;
				}
			}
			catch (...)
			{
			}

			return "";
		}

		std::string ZipFile::getFileMd5(const std::string &name)
		{
			if (mZipFile == nullptr)
				return "";

			MD5 md5 = MD5();
			Utils::Zip::zip_callback func = [](void *pOpaque, unsigned long long ofs, const void *pBuf, size_t n) { ((MD5*)pOpaque)->update((const char *)pBuf, n); return n; };
			if (readBuffered(name, func, &md5))
			{
				md5.finalize();
				return md5.hexdigest();
			}

			return "";
		}

	} // Zip::

} // Utils::
