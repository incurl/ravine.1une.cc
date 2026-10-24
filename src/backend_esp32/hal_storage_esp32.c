/*
 * hal_storage_esp32.c — ESP32-S3 storage backend.
 *
 * The firmware mounts a LittleFS partition at /sketches (the "sketches"
 * partition in partitions.csv) and then serves the generic file API
 * over POSIX calls on that mount.
 *
 * The legacy module main/storage/storage.c owns the *mount* and the
 * device-specific sketch format (patterns.bin, samples.bin, chain.bin,
 * meta.bin, sketches.lst). This backend deliberately knows nothing
 * about that format: it is a generic file service, and the sketch
 * semantics stay where they are.
 *
 * Paths
 * -----
 * The HAL uses absolute paths rooted at the storage root, e.g.
 * "/sketches/0000/meta.bin". On the firmware that path is already the
 * LittleFS mount point, so the mapping is the identity.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_storage.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include "esp_err.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "main/storage/storage.h"

static const char *TAG = "hal_store";

static bool s_mounted;

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_storage_init(void)
{
    if (s_mounted) {
        return SYS_OK;
    }

    /* The legacy module registers and mounts the LittleFS partition. */
    esp_err_t err = storage_init();
    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "storage_init failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }
    err = storage_mount();
    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "storage_mount failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }

    s_mounted = true;
    HAL_LOG_INFO(TAG, "storage HAL ready: LittleFS at %s", SYNTH_STORAGE_ROOT);
    return SYS_OK;
}

sys_status_t hal_storage_deinit(void)
{
    /* The LittleFS mount is owned by the legacy module and is unmounted
     * at shutdown; there is nothing to release here beyond the flag. */
    s_mounted = false;
    return SYS_OK;
}

bool hal_storage_is_mounted(void)
{
    return s_mounted;
}

/* ─── Path handling ─────────────────────────────────────────────── */

/* Reject paths that could escape the storage root. */
static bool path_is_valid(const char *path)
{
    if (path == NULL || path[0] != '/') {
        return false;
    }
    if (strstr(path, "..") != NULL) {
        return false;
    }
    if (strchr(path, '\\') != NULL) {
        return false;
    }
    return true;
}

/* Create every parent directory of a file path. */
static sys_status_t make_parents(const char *path)
{
    char buf[256];
    size_t len = strlen(path);
    if (len >= sizeof(buf)) {
        return SYS_ERR_INVALID_ARG;
    }
    memcpy(buf, path, len + 1);

    /* Walk to the last '/', then create each prefix. */
    for (char *p = buf + 1; *p != '\0'; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
            HAL_LOG_WARN(TAG, "mkdir(%s) failed: errno=%d", buf, errno);
            return SYS_ERR_IO;
        }
        *p = '/';
    }
    return SYS_OK;
}

sys_status_t hal_storage_resolve(const char *path, char *buf, size_t capacity)
{
    if (path == NULL || buf == NULL || capacity == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    if (!path_is_valid(path)) {
        return SYS_ERR_INVALID_ARG;
    }
    size_t len = strlen(path);
    if (len + 1 > capacity) {
        return SYS_ERR_INVALID_ARG;
    }
    /* On the firmware the logical path is already the mount point. */
    memcpy(buf, path, len + 1);
    return SYS_OK;
}

/* ─── Read / write ──────────────────────────────────────────────── */

sys_status_t hal_storage_write(const char *path,
                                const void *data,
                                size_t length,
                                size_t *out_size)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path) || (data == NULL && length > 0)) {
        return SYS_ERR_INVALID_ARG;
    }

    sys_status_t st = make_parents(path);
    if (st != SYS_OK) {
        return st;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        HAL_LOG_ERROR(TAG, "open(%s) failed: errno=%d", path, errno);
        return SYS_ERR_IO;
    }

    size_t written = 0;
    while (written < length) {
        ssize_t n = write(fd, (const uint8_t *)data + written, length - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            HAL_LOG_ERROR(TAG, "write(%s) failed: errno=%d", path, errno);
            return SYS_ERR_IO;
        }
        written += (size_t)n;
    }
    close(fd);

    if (out_size != NULL) {
        *out_size = written;
    }
    return SYS_OK;
}

sys_status_t hal_storage_append(const char *path,
                                const void *data,
                                size_t length,
                                size_t *out_size)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path) || (data == NULL && length > 0)) {
        return SYS_ERR_INVALID_ARG;
    }

    sys_status_t st = make_parents(path);
    if (st != SYS_OK) {
        return st;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        return SYS_ERR_IO;
    }

    size_t written = 0;
    while (written < length) {
        ssize_t n = write(fd, (const uint8_t *)data + written, length - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return SYS_ERR_IO;
        }
        written += (size_t)n;
    }
    close(fd);

    if (out_size != NULL) {
        *out_size = written;
    }
    return SYS_OK;
}

sys_status_t hal_storage_read(const char *path,
                               void *buf,
                               size_t capacity,
                               size_t *out_read)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path) || buf == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    size_t total = 0;
    for (;;) {
        if (total >= capacity) {
            break;
        }
        ssize_t n = read(fd, (uint8_t *)buf + total, capacity - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return SYS_ERR_IO;
        }
        if (n == 0) {
            break;                     /* end of file */
        }
        total += (size_t)n;
    }
    close(fd);

    if (out_read != NULL) {
        *out_read = total;
    }
    return SYS_OK;
}

/* ─── Stat / delete / directories ───────────────────────────────── */

sys_status_t hal_storage_stat(const char *path, hal_file_stat_t *out)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path) || out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    out->size_bytes     = (uint64_t)st.st_size;
    out->modified_unix  = (int64_t)st.st_mtime;
    out->is_directory   = S_ISDIR(st.st_mode) ? true : false;
    return SYS_OK;
}

bool hal_storage_exists(const char *path)
{
    if (!s_mounted || !path_is_valid(path)) {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0;
}

sys_status_t hal_storage_delete(const char *path)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path)) {
        return SYS_ERR_INVALID_ARG;
    }
    if (unlink(path) != 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_storage_make_dir(const char *path)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path)) {
        return SYS_ERR_INVALID_ARG;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_storage_remove_dir(const char *path)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path)) {
        return SYS_ERR_INVALID_ARG;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    /* Remove the contents first, then the directory itself. */
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        char child[256];
        int n = snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(child)) {
            continue;
        }
        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
            hal_storage_remove_dir(child);
        } else {
            unlink(child);
        }
    }
    closedir(dir);

    if (rmdir(path) != 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_storage_list(const char *path,
                               char *names,
                               size_t capacity,
                               size_t *out_count)
{
    if (!s_mounted) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!path_is_valid(path)) {
        return SYS_ERR_INVALID_ARG;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    size_t count = 0;
    size_t written = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        count++;
        if (names != NULL && written < capacity) {
            char *slot = names + (written * HAL_STORAGE_NAME_MAX);
            strncpy(slot, ent->d_name, HAL_STORAGE_NAME_MAX - 1);
            slot[HAL_STORAGE_NAME_MAX - 1] = '\0';
            written++;
        }
    }
    closedir(dir);

    if (out_count != NULL) {
        *out_count = count;
    }
    return SYS_OK;
}

#endif /* BOARD_ESP32S3 */
