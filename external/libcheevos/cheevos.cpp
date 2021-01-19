#include "cheevos.h"
#include "rcheevos/include/rhash.h"
#include "rcheevos/include/rconsoles.h"
#include "libretro-common/include/formats/cdfs.h"

#include <cstdlib>
#include <memory>

#define CHEEVOS_FREE(p) do { void* q = (void*)p; if (q) std::free(q); } while (0)

static void* rc_hash_handle_cd_open_track(const char* path, uint32_t track)
{
	cdfs_track_t* cdfs_track;

	if (track == 0)
		cdfs_track = cdfs_open_data_track(path);
	else
		cdfs_track = cdfs_open_track(path, track);

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

static rc_hash_cdreader cdreader;
static bool cdreaderinit = false;

bool generateHashFromFile(char hash[33], int console_id, const char* path)
{
	try
	{
		if (!cdreaderinit)
		{
			cdreader.open_track = rc_hash_handle_cd_open_track;
			cdreader.read_sector = rc_hash_handle_cd_read_sector;
			cdreader.close_track = rc_hash_handle_cd_close_track;
			cdreader.absolute_sector_to_track_sector = nullptr;
			rc_hash_init_custom_cdreader(&cdreader);
			cdreaderinit = true;
		}
		return rc_hash_generate_from_file(hash, console_id, path) != 0;
	}
	catch (...)
	{
	}

	return false;
}
