// idevfs.h: Common tasks for idevice/house_arrest/afc

#ifndef __IASYNC_IDEVFS_H
#define __IASYNC_IDEVFS_H

#define _XOPEN_SOURCE 700

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "cmds.h"

#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/libimobiledevice.h>

struct stat64;

#include <sys/stat.h>
#include <sys/types.h>

#define IOS_PATH_MAX 4096

typedef struct _idevfs_t
{
    idevice_t dev;
    lockdownd_client_t lockdown;
    lockdownd_service_descriptor_t ha_svc;
    house_arrest_client_t ha;
    afc_client_t afc;
    char cwd[IOS_PATH_MAX];
} idevfs_t;

/// @brief Perform usual setup of an idevice, configuring HA/AFC on the given
/// Bundle ID.
/// @param ga Global args passed from `main()`.
/// @param app_id The application ID to connect to.
/// @return The `idevfs_t` structure for the device on success, or NULL
/// otherwise.
idevfs_t*
idevfs_setup(globargs_t* ga, const char* app_id);

/// @brief Frees an `idevfs_t` structure created by `idevfs_setup`.
void
idevfs_free(idevfs_t* idfs);

/// @brief Creates an absolute path from the given path string.
/// @param idfs The `idevfs_t` structure that will provide the current working
/// directory.
/// @param path The path to turn into an absolute path.
/// @return An absolute path that may or may not exist on the remote device, or
/// NULL if an error occurred.
char*
idevfs_canonpath(idevfs_t* idfs, const char* path);

/// @brief "Changes" the directory of the idevfs device.
/// @param idfs The `idevfs_t` structure.
/// @param dir The path to "change" to.
/// @return `AFC_E_SUCCESS` on success, or an error otherwise.
afc_error_t
idevfs_chdir(idevfs_t* idfs, const char* dir);

/// @brief Reads the given directory on the idevfs device.
/// @param idfs The `idevfs_t` structure.
/// @param dir The path to read, relative to the current directory.
/// @param list A pointer to populate with the list of absolute paths.
/// @param len If not NULL, a pointer to a `size_t` where the list size will be
/// stored.
/// @return `AFC_E_SUCCESS` on success, or an error otherwise.
afc_error_t
idevfs_readdir(idevfs_t* idfs, const char* dir, char*** list, int* len);

afc_error_t
idevfs_stat(idevfs_t* idfs, const char* path, struct stat64* st);

/// @brief Makes the given directory and all parents.
/// @param idfs The `idevfs_t` structure.
/// @param p The path to create. All parent directories will also be created.
/// @return `AFC_E_SUCCESS` on success, or an error otherwise.
afc_error_t
idevfs_mkpath(idevfs_t* idfs, const char* p);

void
idevfs_list_free(char** list);

/// @brief Makes a path relative to the common app Documents folder.
char*
make_docs_path(const char* path);

/// @brief Takes a potential remote path and splits it into the App Bundle ID
/// and absolute app path.
/// @param arg The potential remote path argument.
/// @param app_id Pointer where the App ID string will be stored. This must be
/// freed by the caller.
/// @param path Pointer where the remote path string will be stored. This must
/// be freed by the caller.
/// @return 0 on success, -1 otherwise.
int
split_appid_path(const char* arg, char** app_id, char** path);

#endif /* !__IASYNC_IDEVFS_H */
