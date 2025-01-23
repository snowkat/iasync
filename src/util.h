// util.h: Shared utility functions

#ifndef __IASYNC_UTIL_H
#define __IASYNC_UTIL_H

#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/libimobiledevice.h>

#define UNUSED(x) (void)(x)

/// @brief Gets the lesser of the two integers.
#define MIN(a, b) ((/*CONSTCOND*/ (a) < (b)) ? (a) : (b))
/// @brief Gets the greater of the two integers.
#define MAX(a, b) ((/*CONSTCOND*/ (a) > (b)) ? (a) : (b))

#define UNKNOWN_ID "???"
// Allows for printing strings that might be NULL.
#define STRING_OR_UNKNOWN(x) ((x) != NULL) ? (x) : UNKNOWN_ID

// Dies if ptr is NULL.
#define ALLOC_ASSERT(ptr)                                                      \
    if ((ptr) == NULL) {                                                       \
        log_printf(IA_CRITICAL, "%s: Memory allocation error!\n", __func__);   \
        abort();                                                               \
    }

#include <libimobiledevice/libimobiledevice.h>

struct _global_args;

/// @brief Gets an `idevice_t` using the global args provided by the user.
/// @param ga Global argument struct provided to cmd function.
/// @return `idevice_t` if the device is available, or NULL.
idevice_t
get_idevice_by_globargs(struct _global_args* ga);

/// @brief Gets the name of the provided device.
/// @param device The device to get the name of.
/// @return The name as a string, or NULL if unavailable. The return value must
/// be freed by the caller.
char*
get_idevice_name(idevice_t device);

char*
join_path(const char* p1, const char* p2);

/**
 * @brief Provides an allocated version of the new substring.
 *
 * @param root The main string to splice.
 * @param begin The index to begin the splice.
 * @param end The index to end the splice, or -1 to go until the string ends.
 * @return An allocated string representing the substring.
 */
char*
substr(const char* root, int begin, int end);

#endif /* !__IASYNC_UTIL_H */