#include "cheevos.h"
#include "rcheevos/include/rc_hash.h"
#include "rcheevos/include/rc_consoles.h"
#include "libretro-common/include/formats/cdfs.h"
#include "libretro-common/include/string/stdstring.h"
#include "libretro-common/include/file/file_path.h"
#include "libretro.h"

#include <cstdlib>
#include <memory>

#ifdef HAVE_CHD
#include "libretro-common/include/streams/chd_stream.h"
#endif

static bool readersinit = false;

#define CHEEVOS_FREE(p) do { void* q = (void*)p; if (q) std::free(q); } while (0)

#ifdef HAVE_CHD
static void* rc_hash_handle_chd_open_track(
    const char* path, uint32_t track)
{
    cdfs_track_t* cdfs_track;

    switch (track)
    {
    case RC_HASH_CDTRACK_FIRST_DATA:
        cdfs_track = cdfs_open_data_track(path);
        break;

    case RC_HASH_CDTRACK_LAST:
        cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_LAST);
        break;

    case RC_HASH_CDTRACK_LARGEST:
        cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY);
        break;

    default:
        cdfs_track = cdfs_open_track(path, track);
        break;
    }

    if (cdfs_track)
    {
        cdfs_file_t* file = (cdfs_file_t*)malloc(sizeof(cdfs_file_t));
        if (cdfs_open_file(file, cdfs_track, NULL))
            return file; /* ASSERT: file owns cdfs_track now */

        CHEEVOS_FREE(file);
        cdfs_close_track(cdfs_track); /* ASSERT: this free()s cdfs_track */
    }

    return NULL;
}

static size_t rc_hash_handle_chd_read_sector(
    void* track_handle, uint32_t sector,
    void* buffer, size_t requested_bytes)
{
    cdfs_file_t* file = (cdfs_file_t*)track_handle;
    uint32_t track_sectors = cdfs_get_num_sectors(file);

    sector -= cdfs_get_first_sector(file);
    if (sector >= track_sectors)
        return 0;

    cdfs_seek_sector(file, sector);
    return cdfs_read_file(file, buffer, requested_bytes);
}

static uint32_t rc_hash_handle_chd_first_track_sector(
    void* track_handle)
{
    cdfs_file_t* file = (cdfs_file_t*)track_handle;
    return cdfs_get_first_sector(file);
}

static void rc_hash_handle_chd_close_track(void* track_handle)
{
    cdfs_file_t* file = (cdfs_file_t*)track_handle;
    if (file)
    {
        cdfs_close_track(file->track);
        cdfs_close_file(file); /* ASSERT: this does not free() file */
        CHEEVOS_FREE(file);
    }
}
#endif

static void* rc_hash_handle_file_open(const char* path)
{
    return intfstream_open_file(path,
        RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void rc_hash_handle_file_seek(
    void* file_handle, int64_t offset, int origin)
{
    intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static int64_t rc_hash_handle_file_tell(void* file_handle)
{
    return intfstream_tell((intfstream_t*)file_handle);
}

static size_t rc_hash_handle_file_read(
    void* file_handle, void* buffer, size_t requested_bytes)
{
    return intfstream_read((intfstream_t*)file_handle,
        buffer, requested_bytes);
}

static void rc_hash_handle_file_close(void* file_handle)
{
    intfstream_close((intfstream_t*)file_handle);
    CHEEVOS_FREE(file_handle);
}

static void* rc_hash_handle_cd_open_track(
    const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
    rc_hash_callbacks_t* callbacks = (rc_hash_callbacks_t*)&iterator->callbacks;
    callbacks->filereader.open = rc_hash_handle_file_open;
    callbacks->filereader.seek = rc_hash_handle_file_seek;
    callbacks->filereader.tell = rc_hash_handle_file_tell;
    callbacks->filereader.read = rc_hash_handle_file_read;
    callbacks->filereader.close = rc_hash_handle_file_close;

    if (string_is_equal_noncase(path_get_extension(path), "chd"))
    {
#ifdef HAVE_CHD
        /* special handlers for CHD file */
        callbacks->cdreader.open_track_iterator = rc_hash_handle_cd_open_track;
        callbacks->cdreader.read_sector = rc_hash_handle_chd_read_sector;
        callbacks->cdreader.close_track = rc_hash_handle_chd_close_track;
        callbacks->cdreader.first_track_sector = rc_hash_handle_chd_first_track_sector;

        return rc_hash_handle_chd_open_track(path, track);
#else
        CHEEVOS_LOG(RCHEEVOS_TAG "Cannot generate hash from CHD without HAVE_CHD compile flag\n");
        return NULL;
#endif
    }
    else
    {
        /* not a CHD file, use the default handlers */
        struct rc_hash_cdreader cdreader;
        rc_hash_get_default_cdreader(&cdreader);
        return cdreader.open_track_iterator(path, track, iterator);
    }
}

bool generateHashFromFile(char hash[33], int console_id, const char* path)
{
	try
	{
		if (!readersinit)
		{
            struct rc_hash_cdreader cdreader;
            rc_hash_get_default_cdreader(&cdreader);
            cdreader.open_track_iterator = rc_hash_handle_cd_open_track;
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
