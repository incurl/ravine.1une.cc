/*
 * sys_status.c — status code names.
 *
 * Shared by both targets so that log output and error handling read
 * identically on the device and on the host.
 */
#include "common/sys_status.h"

const char *sys_status_str(sys_status_t status)
{
    switch (status) {
    case SYS_OK:                 return "SYS_OK";
    case SYS_ERR_INVALID_ARG:    return "SYS_ERR_INVALID_ARG";
    case SYS_ERR_INVALID_STATE:  return "SYS_ERR_INVALID_STATE";
    case SYS_ERR_NO_MEM:         return "SYS_ERR_NO_MEM";
    case SYS_ERR_NOT_FOUND:      return "SYS_ERR_NOT_FOUND";
    case SYS_ERR_IO:             return "SYS_ERR_IO";
    case SYS_ERR_NOT_SUPPORTED:  return "SYS_ERR_NOT_SUPPORTED";
    case SYS_ERR_TIMEOUT:        return "SYS_ERR_TIMEOUT";
    case SYS_ERR_BUSY:           return "SYS_ERR_BUSY";
    case SYS_ERR_STATUS_COUNT:   break;
    }
    return "SYS_ERR_UNKNOWN";
}
