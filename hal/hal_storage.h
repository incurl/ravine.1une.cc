/*
 * hal_storage.h — persistent file storage abstraction.
 *
 * The firmware backend mounts a LittleFS partition; the simulator
 * backend maps the same paths into a local directory. The path API is
 * identical on both, so application code that reads a sketch works
 * unchanged on the PC.
 *
 * Path model:
 *   - Paths are absolute and POSIX-style: "/sketches/0000/meta.bin".
 *   - The leading '/' is the storage root, which maps to the mounted
 *     LittleFS partition on the firmware and to SYNTH_SIM_FLASH_DIR on
 *     the host. The host backend normalises paths so that both
 *     "/sketches/x" and "sketches/x" address the same file.
 *   - No "..", no backslashes, no trailing-slash surprises.
 *
 * The storage HAL provides *generic file* access. Device-specific
 * concepts — sketches, patterns, the sample pool — stay in the
 * firmware's own storage module; they are not part of this interface.
 *
 * Threading: all calls are blocking. Keep them off the real-time audio
 * path. The firmware backend serialises access internally.
 */
#ifndef SYNTH_HAL_STORAGE_H
#define SYNTH_HAL_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Metadata about one stored file. */
typedef struct {
    uint64_t size_bytes;
    int64_t  modified_unix;   /* seconds since the Unix epoch; 0 if unknown */
    bool     is_directory;
} hal_file_stat_t;

/**
 * Initialise the storage backend and make the root available.
 *
 * On the firmware this mounts the LittleFS partition. On the host this
 * creates the local directory if needed.
 *
 * Returns SYS_OK, SYS_ERR_IO, or SYS_ERR_NO_MEM.
 */
sys_status_t hal_storage_init(void);

/**
 * Release the storage backend (unmount on the firmware).
 */
sys_status_t hal_storage_deinit(void);

/**
 * @return true when the storage root is mounted and usable.
 */
bool hal_storage_is_mounted(void);

/**
 * Write a file, replacing any existing content.
 *
 * @param path        Absolute path.
 * @param data        Bytes to write; may be NULL only if length is 0.
 * @param length      Number of bytes.
 * @param out_size    Optional; receives the number of bytes written.
 *
 * Parent directories are created as needed.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_IO, or SYS_ERR_NO_MEM.
 */
sys_status_t hal_storage_write(const char *path,
                                const void *data,
                                size_t length,
                                size_t *out_size);

/**
 * Read a file.
 *
 * @param path     Absolute path.
 * @param buf      Destination buffer.
 * @param capacity Size of buf in bytes.
 * @param out_read Optional; receives the number of bytes read.
 *
 * Reads up to @p capacity bytes; a short read means end of file.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_NOT_FOUND, or
 * SYS_ERR_IO.
 */
sys_status_t hal_storage_read(const char *path,
                               void *buf,
                               size_t capacity,
                               size_t *out_read);

/**
 * Append to a file, creating it if necessary.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_IO, or SYS_ERR_NO_MEM.
 */
sys_status_t hal_storage_append(const char *path,
                                const void *data,
                                size_t length,
                                size_t *out_size);

/**
 * Stat a path.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_NOT_FOUND, or
 * SYS_ERR_IO.
 */
sys_status_t hal_storage_stat(const char *path, hal_file_stat_t *out);

/**
 * @return true if the path exists.
 */
bool hal_storage_exists(const char *path);

/**
 * Delete a file.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_NOT_FOUND, or
 * SYS_ERR_IO.
 */
sys_status_t hal_storage_delete(const char *path);

/**
 * Create a directory, including any missing parents.
 *
 * Succeeds if the directory already exists.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_IO.
 */
sys_status_t hal_storage_make_dir(const char *path);

/**
 * Remove a directory and everything under it.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_NOT_FOUND, or
 * SYS_ERR_IO.
 */
sys_status_t hal_storage_remove_dir(const char *path);

/**
 * Maximum length of a single directory entry name, including the NUL.
 *
 * The listing buffer is an array of fixed-width slots, each
 * HAL_STORAGE_NAME_MAX bytes wide.
 */
#define HAL_STORAGE_NAME_MAX 256

/**
 * List the entries of a directory.
 *
 * @param path      Directory to list.
 * @param names     Array of slots, each HAL_STORAGE_NAME_MAX bytes
 *                  wide, receiving the entry names (not full paths).
 *                  May be NULL to only count the entries.
 * @param capacity  Maximum number of slots in @p names.
 * @param out_count Receives the total number of entries in the
 *                  directory, which may exceed @p capacity.
 *
 * Entry names are bare names without path separators. Slots beyond
 * @p capacity are left untouched.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_NOT_FOUND, or
 * SYS_ERR_IO.
 */
sys_status_t hal_storage_list(const char *path,
                               char *names,
                               size_t capacity,
                               size_t *out_count);

/**
 * Format the storage root as a path the host backend understands.
 *
 * @param path     The logical path, e.g. "/sketches/0000".
 * @param buf      Destination buffer.
 * @param capacity Size of buf.
 *
 * On the firmware this is the identity mapping. On the host this
 * rewrites the leading '/' into the simulator flash directory.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_storage_resolve(const char *path,
                                 char *buf,
                                 size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_STORAGE_H */
