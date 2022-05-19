#include "cheevos.h"
#include "rcheevos/include/rc_hash.h"
#include "rcheevos/include/rc_consoles.h"
#include "libretro-common/include/formats/cdfs.h"
#include "libretro.h"

#include <cstdlib>
#include <memory>

#ifdef HAVE_CHD
#include "libretro-common/include/streams/chd_stream.h"
#endif

#define CHEEVOS_FREE(p) do { void* q = (void*)p; if (q) std::free(q); } while (0)

void* rc_hash_handle_file_open(const char* path)
{
	return intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

void rc_hash_handle_file_seek(void* file_handle, int64_t offset, int origin)
{
	intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

int64_t rc_hash_handle_file_tell(void* file_handle)
{
	return intfstream_tell((intfstream_t*)file_handle);
}

size_t rc_hash_handle_file_read(void* file_handle, void* buffer, size_t requested_bytes)
{
	return intfstream_read((intfstream_t*)file_handle, buffer, requested_bytes);
}

void rc_hash_handle_file_close(void* file_handle)
{
	intfstream_close((intfstream_t*)file_handle);
	CHEEVOS_FREE(file_handle);
}

static void* rc_hash_handle_cd_open_track(const char* path, uint32_t track)
{
	cdfs_track_t* cdfs_track;

	switch (track)
	{
	case RC_HASH_CDTRACK_FIRST_DATA:
		cdfs_track = cdfs_open_data_track(path);
		break;
#ifdef HAVE_CHD
	case RC_HASH_CDTRACK_LAST:
		cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_LAST);
		break;
	case RC_HASH_CDTRACK_LARGEST:
		cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY);
		break;
#endif
	default:
		cdfs_track = cdfs_open_track(path, track);
		break;
	}

	if (cdfs_track)
	{
		cdfs_file_t* file = (cdfs_file_t*)std::malloc(sizeof(cdfs_file_t));
		if (cdfs_open_file(file, cdfs_track, NULL))
			return file;

		CHEEVOS_FREE(file);
	}

	cdfs_close_track(cdfs_track); /* ASSERT: this free()s cdfs_track */
	return NULL;
}

static size_t rc_hash_handle_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
	cdfs_file_t* file = (cdfs_file_t*)track_handle;

	uint32_t track_sectors = cdfs_get_num_sectors(file);

	sector -= cdfs_get_first_sector(file);
	if (sector >= track_sectors)
		return 0;

	cdfs_seek_sector(file, sector);
	return cdfs_read_file(file, buffer, requested_bytes);
}

static void rc_hash_handle_cd_close_track(void* track_handle)
{
	cdfs_file_t* file = (cdfs_file_t*)track_handle;
	if (file)
	{
		cdfs_close_track(file->track);
		cdfs_close_file(file); /* ASSERT: this does not free() file */
		CHEEVOS_FREE(file);
	}
}

static uint32_t rc_hash_handle_cd_first_track_sector(void* track_handle)
{
	cdfs_file_t* file = (cdfs_file_t*)track_handle;
	return cdfs_get_first_sector(file);
}

static rc_hash_filereader filereader;
static rc_hash_cdreader cdreader;
static bool readersinit = false;

bool generateHashFromFile(char hash[33], int console_id, const char* path)
{
	try
	{
		if (!readersinit)
		{

			filereader.open = rc_hash_handle_file_open;
			filereader.seek = rc_hash_handle_file_seek;
			filereader.tell = rc_hash_handle_file_tell;
			filereader.read = rc_hash_handle_file_read;
			filereader.close = rc_hash_handle_file_close;
			rc_hash_init_custom_filereader(&filereader);

			cdreader.open_track = rc_hash_handle_cd_open_track;
			cdreader.read_sector = rc_hash_handle_cd_read_sector;
			cdreader.close_track = rc_hash_handle_cd_close_track;
			cdreader.first_track_sector = rc_hash_handle_cd_first_track_sector;
//			cdreader.absolute_sector_to_track_sector = nullptr;
			rc_hash_init_custom_cdreader(&cdreader);

			readersinit = true;
		}

		return rc_hash_generate_from_file(hash, console_id, path) != 0;
	}
	catch (...)
	{
	}

	return false;
}
