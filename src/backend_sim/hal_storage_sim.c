/*
 * hal_storage_sim.c — SDL2 storage backend (host simulator).
 *
 * Maps the device's storage root onto a local directory, so that code
 * which reads and writes sketches on the firmware reads and writes the
 * same files on the PC.
 *
 * Path mapping
 * ------------
 * The device root is "/sketches" (SYNTH_STORAGE_ROOT), which is the
 * LittleFS mount point on the firmware. On the host it becomes
 * "./sim_flash/sketches", under SYNTH_SIM_FLASH_DIR.
 *
 * The mapping is deliberately tolerant: "/sketches/x", "sketches/x" and
 * "./sketches/x" all address the same file. The device always uses
 * the absolute form, but accepting the others costs nothing and makes
 * hand-written test paths behave predictably.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_storage.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char *TAG = "hal_store_sim";

static bool s_ready;

/* ─── Path mapping ──────────────────────────────────────────────── */

/* Strip any leading "./" and "/" from a logical path and return the
 * remainder, or NULL if the path is invalid. */
static const char *logical_relative(const char *path)
{
    if (path == NULL) {
        return NULL;
    }
    while (*path == '/') {
        path++;
    }
    if (strncmp(path, "./", 2) == 0) {
        path += 2;
    }
    if (path[0] == '\0') {
        return NULL;
    }
    /* Reject anything that could escape the storage root. */
    if (strstr(path, "..") != NULL) {
        return NULL;
    }
    if (strchr(path, '\\') != NULL) {
        return NULL;
    }
    return path;
}

/* Build the host path for a logical path. */
static sys_status_t host_path(const char *path, char *buf, size_t capacity)
{
    const char *rel = logical_relative(path);
    if (rel == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    int n = snprintf(buf, capacity, "%s/%s", SYNTH_SIM_FLASH_DIR, rel);
    if (n < 0 || (size_t)n >= capacity) {
        return SYS_ERR_INVALID_ARG;
    }
    return SYS_OK;
}

/* Create every parent directory of a host path. */
static sys_status_t make_parents(const char *host)
{
    char buf[512];
    size_t len = strlen(host);
    if (len >= sizeof(buf)) {
        return SYS_ERR_INVALID_ARG;
    }
    memcpy(buf, host, len + 1);

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

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_storage_init(void)
{
    if (s_ready) {
        return SYS_OK;
    }

    /* Create the flash directory and the device root inside it. */
    if (mkdir(SYNTH_SIM_FLASH_DIR, 0755) != 0 && errno != EEXIST) {
        HAL_LOG_ERROR(TAG, "mkdir(%s) failed: errno=%d",
                      SYNTH_SIM_FLASH_DIR, errno);
        return SYS_ERR_IO;
    }

    char root[512];
    int n = snprintf(root, sizeof(root), "%s/sketches", SYNTH_SIM_FLASH_DIR);
    if (n < 0 || (size_t)n >= sizeof(root)) {
        return SYS_ERR_INVALID_ARG;
    }
    if (mkdir(root, 0755) != 0 && errno != EEXIST) {
        HAL_LOG_ERROR(TAG, "mkdir(%s) failed: errno=%d", root, errno);
        return SYS_ERR_IO;
    }

    s_ready = true;
    HAL_LOG_INFO(TAG, "storage HAL ready: %s (device root %s)",
                 SYNTH_SIM_FLASH_DIR, SYNTH_STORAGE_ROOT);
    return SYS_OK;
}

sys_status_t hal_storage_deinit(void)
{
    /* The files stay on disk: the simulator's flash is persistent, just
     * like the device's LittleFS partition. */
    s_ready = false;
    return SYS_OK;
}

bool hal_storage_is_mounted(void)
{
    return s_ready;
}

sys_status_t hal_storage_resolve(const char *path, char *buf, size_t capacity)
{
    if (buf == NULL || capacity == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    return host_path(path, buf, capacity);
}


/* ─── Read / write ──────────────────────────────────────────────── */

sys_status_t hal_storage_write(const char *path,
                                const void *data,
                                size_t length,
                                size_t *out_size)
{
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    if (data == NULL && length > 0) {
        return SYS_ERR_INVALID_ARG;
    }

    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }
    st = make_parents(host);
    if (st != SYS_OK) {
        return st;
    }

    FILE *f = fopen(host, "wb");
    if (f == NULL) {
        HAL_LOG_ERROR(TAG, "fopen(%s) failed: errno=%d", host, errno);
        return SYS_ERR_IO;
    }
    size_t written = (length > 0) ? fwrite(data, 1, length, f) : 0;
    fclose(f);

    if (written != length) {
        return SYS_ERR_IO;
    }
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
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    if (data == NULL && length > 0) {
        return SYS_ERR_INVALID_ARG;
    }

    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }
    st = make_parents(host);
    if (st != SYS_OK) {
        return st;
    }

    FILE *f = fopen(host, "ab");
    if (f == NULL) {
        return SYS_ERR_IO;
    }
    size_t written = (length > 0) ? fwrite(data, 1, length, f) : 0;
    fclose(f);

    if (written != length) {
        return SYS_ERR_IO;
    }
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
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    if (buf == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }

    FILE *f = fopen(host, "rb");
    if (f == NULL) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }
    size_t got = fread(buf, 1, capacity, f);
    fclose(f);

    if (out_read != NULL) {
        *out_read = got;
    }
    return SYS_OK;
}

/* ─── Stat / delete / directories ───────────────────────────────── */

sys_status_t hal_storage_stat(const char *path, hal_file_stat_t *out)
{
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }

    struct stat sb;
    if (stat(host, &sb) != 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    out->size_bytes    = (uint64_t)sb.st_size;
    out->modified_unix = (int64_t)sb.st_mtime;
    out->is_directory  = S_ISDIR(sb.st_mode) ? true : false;
    return SYS_OK;
}

bool hal_storage_exists(const char *path)
{
    if (!s_ready) {
        return false;
    }
    char host[512];
    if (host_path(path, host, sizeof(host)) != SYS_OK) {
        return false;
    }
    struct stat sb;
    return stat(host, &sb) == 0;
}

sys_status_t hal_storage_delete(const char *path)
{
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }
    if (unlink(host) != 0) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_storage_make_dir(const char *path)
{
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }
    st = make_parents(host);
    if (st != SYS_OK) {
        return st;
    }
    if (mkdir(host, 0755) != 0 && errno != EEXIST) {
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_storage_remove_dir(const char *path)
{
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }
    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }

    DIR *dir = opendir(host);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        char child[640];
        int n = snprintf(child, sizeof(child), "%s/%s", host, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(child)) {
            continue;
        }
        struct stat sb;
        if (stat(child, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            hal_storage_remove_dir(child);
        } else {
            unlink(child);
        }
    }
    closedir(dir);

    if (rmdir(host) != 0) {
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
    if (!s_ready) {
        return SYS_ERR_INVALID_STATE;
    }

    char host[512];
    sys_status_t st = host_path(path, host, sizeof(host));
    if (st != SYS_OK) {
        return st;
    }

    DIR *dir = opendir(host);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return SYS_ERR_NOT_FOUND;
        }
        return SYS_ERR_IO;
    }

    size_t count   = 0;
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

#endif /* BOARD_SIMULATOR */
