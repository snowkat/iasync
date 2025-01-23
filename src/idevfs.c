// idevfs.c
#include "config.h"

#include "idevfs.h"
#include "log.h"
#include "util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define HA_CMD_DOCUMENTS "VendDocuments"
#define DOCUMENTS_ROOT "/Documents"

idevfs_t*
idevfs_setup(globargs_t* ga, const char* app_id)
{
    house_arrest_error_t ha_err;
    lockdownd_error_t ld_err;
    afc_error_t afc_err;
    idevfs_t* idfs;
    plist_t res, err_node;

    if (ga == NULL || app_id == NULL)
        return NULL;

    idfs = calloc(1, sizeof(idevfs_t));
    if (idfs == NULL) {
        log_printf(IA_ERROR, "Memory allocation error!\n");
        return NULL;
    }

    idfs->dev = get_idevice_by_globargs(ga);
    if (idfs->dev == NULL) {
        log_printf(IA_ERROR, "Couldn't find a device to connect to.\n");
        idevfs_free(idfs);
        return NULL;
    }

    ld_err = lockdownd_client_new_with_handshake(
      idfs->dev, &(idfs->lockdown), PROJECT_NAME);
    if (ld_err != LOCKDOWN_E_SUCCESS) {
        switch (ld_err) {
        case LOCKDOWN_E_PASSWORD_PROTECTED:
            log_printf(IA_ERROR,
                       "Your device appears to be locked. Please unlock it to "
                       "continue pairing.\n");
            break;
        case LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING:
            log_printf(
              IA_ERROR,
              "Please accept the pairing dialog to continue pairing.\n");
            break;
        case LOCKDOWN_E_USER_DENIED_PAIRING:
            log_printf(IA_ERROR, "Pairing was denied from the device.\n");
            break;
        default:
            log_printf(IA_ERROR, "Pairing failed: Error %d\n", ld_err);
        }
        idevfs_free(idfs);
        return NULL;
    }

    /* TODO: Why does house_arrest_client_start_service not work, but this does?
     */
    ld_err = lockdownd_start_service(
      idfs->lockdown, HOUSE_ARREST_SERVICE_NAME, &(idfs->ha_svc));
    if (ld_err != LOCKDOWN_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't start document sharing service! Error %d\n",
                   ld_err);
        idevfs_free(idfs);
        return NULL;
    } else if (idfs->ha_svc == NULL) {
        log_printf(IA_ERROR,
                   "Document sharing service was started, but we weren't given "
                   "access?\n");
        idevfs_free(idfs);
        return NULL;
    }

    ha_err = house_arrest_client_new(idfs->dev, idfs->ha_svc, &(idfs->ha));
    if (ha_err != HOUSE_ARREST_E_SUCCESS) {
        log_printf(
          IA_ERROR,
          "Couldn't connect to the document transfer service! Error %d\n",
          ha_err);
        idevfs_free(idfs);
        return NULL;
    }

    ha_err = house_arrest_send_command(idfs->ha, HA_CMD_DOCUMENTS, app_id);
    if (ha_err != HOUSE_ARREST_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Error requesting access to documents for %s! Error %d\n",
                   app_id,
                   ha_err);
        idevfs_free(idfs);
        return NULL;
    }

    ha_err = house_arrest_get_result(idfs->ha, &res);
    if (ha_err != HOUSE_ARREST_E_SUCCESS) {
        log_printf(
          IA_ERROR,
          "Couldn't get result from document sharing service! Error %d\n",
          ha_err);
        idevfs_free(idfs);
        return NULL;
    }

    err_node = plist_dict_get_item(res, "Error");
    if (err_node != NULL) {
        char* val;
        plist_get_string_val(err_node, &val);
        log_printf(IA_ERROR, "Document sharing service error: %s\n", val);
        free(val);
    }
    plist_free(res);

    afc_err = afc_client_new_from_house_arrest_client(idfs->ha, &(idfs->afc));
    if (afc_err != AFC_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Error loading file sharing service: %s\n",
                   afc_strerror(afc_err));
        idevfs_free(idfs);
        return NULL;
    }

    strncpy(idfs->cwd, DOCUMENTS_ROOT, strlen(DOCUMENTS_ROOT) + 1);

    return idfs;
}

void
idevfs_free(idevfs_t* idfs)
{
    if (idfs == NULL)
        return;

    if (idfs->afc != NULL) {
        afc_client_free(idfs->afc);
        idfs->afc = NULL;
    }

    if (idfs->ha != NULL) {
        house_arrest_client_free(idfs->ha);
        idfs->ha = NULL;
    }

    if (idfs->ha_svc != NULL) {
        lockdownd_service_descriptor_free(idfs->ha_svc);
        idfs->ha_svc = NULL;
    }

    if (idfs->lockdown != NULL) {
        lockdownd_client_free(idfs->lockdown);
        idfs->lockdown = NULL;
    }

    if (idfs->dev != NULL) {
        idevice_free(idfs->dev);
        idfs->dev = NULL;
    }

    free(idfs);

    return;
}

char*
make_docs_path(const char* path)
{
    char* final;
    int final_len;
    const char* fmt = "/Documents/%s";

    if (path == NULL)
        return NULL;

    final_len = snprintf(NULL, 0, fmt, path);
    if (final_len < 0)
        return NULL;

    final = calloc(final_len + 1, sizeof(char));
    if (final == NULL)
        return NULL;

    snprintf(final, final_len + 1, fmt, path);

    return final;
}

/*
 * Largely taken from NetBSD libc's realpath(), with the exclusion of lstat.
 */
char*
idevfs_canonpath(idevfs_t* idfs, const char* path)
{
    char *p, *resolved;
    const char* q;

    if (idfs == NULL || path == NULL)
        return NULL;

    resolved = malloc(IOS_PATH_MAX);
    if (resolved == NULL)
        return NULL;

    if (*path == '\0') {
        strncpy(resolved, idfs->cwd, IOS_PATH_MAX);
        return resolved;
    }

    /*
     * `p' is where we'll put a new component with prepending
     * a delimiter.
     */
    p = resolved;

    if (*path != '/') {
        p = stpncpy(resolved, idfs->cwd, IOS_PATH_MAX);
    }

loop:
    /* Skip any slash. */
    while (*path == '/')
        path++;

    if (*path == '\0') {
        if (p == resolved)
            *p = '\0';
        return resolved;
    }

    q = path;
    do
        q++;
    while (*q != '/' && *q != '\0');

    /* Test . or .. */
    if (path[0] == '.') {
        if (q - path == 1) {
            path = q;
            goto loop;
        }
        if (path[1] == '.' && q - path == 2) {
            /* Trim the last component. */
            if (p != resolved)
                while (*--p != '/')
                    ;
            path = q;
            goto loop;
        }
    }

    /* Append this component. */
    if (p - resolved + 1 + q - path + 1 > IOS_PATH_MAX) {
        if (p == resolved)
            *p++ = '/';
        *p = '\0';
        goto out;
    }
    p[0] = '/';
    memcpy(&p[1],
           path,
           /* LINTED We know q > path. */
           q - path);
    p[1 + q - path] = '\0';

    /* Advance both resolved and unresolved path. */
    p += 1 + q - path;
    path = q;
    goto loop;
out:
    free(resolved);
    return NULL;
}

afc_error_t
idevfs_chdir(idevfs_t* idfs, const char* dir)
{
    char** info = NULL;
    char* new_dir = NULL;
    afc_error_t err;
    struct stat64 st;

    if (idfs == NULL || dir == NULL)
        return AFC_E_INVALID_ARG;

    new_dir = idevfs_canonpath(idfs, dir);
    if (new_dir == NULL)
        return AFC_E_NO_MEM;

    err = idevfs_stat(idfs, idfs->cwd, &st);
    if (err != AFC_E_SUCCESS) {
        goto done;
    }

    if (S_ISDIR(st.st_mode)) {
        /* OK, this is a dir! */
        afc_dictionary_free(info);
        strncpy(idfs->cwd, new_dir, IOS_PATH_MAX);
        return AFC_E_SUCCESS;
    } else {
        /*
         * AFC_E_NOT_A_DIR doesn't seem to exist, so I guess this is the
         * best we can do?
         */
        err = AFC_E_INVALID_ARG;
    }
done:
    if (new_dir)
        free(new_dir);

    if (info)
        afc_dictionary_free(info);

    return err;
}

/// @brief Performs `strtoull` in a way that tries to avoid overflows.
/// @param in The variable to store the integer.
/// @param str The numeric string to convert to an integer.
#define CHECKED_STRTOU(in, str)                                                \
    {                                                                          \
        const uint64_t _len = sizeof(in) * 8;                                  \
        const uint64_t _max_int =                                              \
          (sizeof(in) >= 8) ? UINT64_MAX : (uint64_t)((1 << (_len + 1)) - 1);  \
        uint64_t _res = strtoull(str, NULL, 10);                               \
        in = (_res > _max_int) ? _max_int : _res;                              \
    }

afc_error_t
idevfs_stat(idevfs_t* idfs, const char* path, struct stat64* st)
{
    char* real_path;
    char** info;
    afc_error_t err;

    if (idfs == NULL || path == NULL || st == NULL)
        return AFC_E_INVALID_ARG;

    real_path = idevfs_canonpath(idfs, path);
    if (real_path == NULL)
        return AFC_E_NO_MEM;

    err = afc_get_file_info(idfs->afc, real_path, &info);
    if (err != AFC_E_SUCCESS) {
        free(real_path);
        return err;
    }

    memset(st, 0, sizeof(struct stat64));

    for (int i = 0; info[i] != NULL; i += 2) {
        const char *key = info[i], *val = info[i + 1];
        if (!strcmp(key, "st_dev")) {
            CHECKED_STRTOU(st->st_dev, val);
        } else if (!strcmp(key, "st_ifmt")) {
            if (!strcmp(val, "S_IFIFO")) {
                st->st_mode = S_IFIFO;
            } else if (!strcmp(val, "S_IFCHR")) {
                st->st_mode = S_IFCHR;
            } else if (!strcmp(val, "S_IFDIR")) {
                st->st_mode = S_IFDIR;
            } else if (!strcmp(val, "S_IFBLK")) {
                st->st_mode = S_IFBLK;
            } else if (!strcmp(val, "S_IFREG")) {
                st->st_mode = S_IFREG;
            } else if (!strcmp(val, "S_IFLNK")) {
                st->st_mode = S_IFLNK;
            } else if (!strcmp(val, "S_IFSOCK")) {
                st->st_mode = S_IFSOCK;
            }
            /*
             * S_IFWHT not supported on Linux and a pretty rare case, so
             * we're skipping it here
             */
        } else if (!strcmp(key, "st_nlink")) {
            CHECKED_STRTOU(st->st_nlink, val);
        } else if (!strcmp(key, "st_ino")) {
            CHECKED_STRTOU(st->st_ino, val);
        } else if (!strcmp(key, "st_uid")) {
            CHECKED_STRTOU(st->st_uid, val);
        } else if (!strcmp(key, "st_gid")) {
            CHECKED_STRTOU(st->st_gid, val);
        } else if (!strcmp(key, "st_size")) {
            CHECKED_STRTOU(st->st_size, val);
        } else if (!strcmp(key, "st_blocks")) {
            CHECKED_STRTOU(st->st_blocks, val);
        } else if (!strcmp(key, "st_blksize")) {
            CHECKED_STRTOU(st->st_blksize, val);
        } else if (!strcmp(key, "st_mtime")) {
            uint64_t ts = strtoull(val, NULL, 10);
            st->st_mtim.tv_sec = ts / 1000000000;
            st->st_mtim.tv_nsec = ts % 1000000000;
        }
    }

    afc_dictionary_free(info);
    free(real_path);

    return AFC_E_SUCCESS;
}

/*
 * Highly adapted from mkpath() in NetBSD src/bin/mkdir/mkdir.c
 */
afc_error_t
idevfs_mkpath(idevfs_t* idfs, const char* p)
{
    struct stat64 st;
    char path[IOS_PATH_MAX] = { 0 };
    char* slash;
    afc_error_t err;

    if (idfs == NULL || p == NULL)
        return AFC_E_INVALID_ARG;

    strncpy(path, p, IOS_PATH_MAX);

    slash = path;

    for (;;) {
        int done;

        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        done = (*(slash + strspn(slash, "/")) == '\0');
        *slash = '\0';

        err = afc_make_directory(idfs->afc, path);
        if (err != AFC_E_SUCCESS) {
            /*
             * Can't create; path exists or no perms.
             * stat() path to determine what's there now.
             */
            afc_error_t stat_err = idevfs_stat(idfs, path, &st);
            if (stat_err != AFC_E_SUCCESS) {
                /* Not there; use make_dir's error */
                return err;
            }
            if (!S_ISDIR(st.st_mode)) {
                /* Is there, but isn't a directory */
                return AFC_E_OBJECT_NOT_FOUND;
            }
        }

        if (done) {
            break;
        }
        *slash = '/';
    }

    return AFC_E_SUCCESS;
}

void
idevfs_list_free(char** list)
{
    for (int i = 0; list[i] != NULL; i++) {
        free(list[i]);
    }
    free(list);
    return;
}

afc_error_t
idevfs_readdir(idevfs_t* idfs, const char* dir, char*** list, int* len)
{
    char** info = NULL;
    char** lp = NULL;
    char* real_path = NULL;
    int ilen;
    afc_error_t err;

    if (idfs == NULL || dir == NULL || list == NULL)
        return AFC_E_INVALID_ARG;

    *list = NULL;

    real_path = idevfs_canonpath(idfs, dir);
    if (real_path == NULL)
        return AFC_E_NO_MEM;

    err = afc_read_directory(idfs->afc, real_path, &info);
    if (err != AFC_E_SUCCESS) {
        goto out;
    }

    /* Pass 1: Get the length of the info list */
    for (ilen = 0; info[ilen] != NULL; ilen++)
        ;

    lp = calloc(ilen + 1, sizeof(char*));
    if (lp == NULL) {
        err = AFC_E_NO_MEM;
        goto out;
    }

    /* Pass 2: Make a list with all "absolute" paths */
    for (int i = 0; i < ilen; i++) {
        lp[i] = join_path(real_path, info[i]);
        if (lp[i] == NULL) {
            err = AFC_E_NO_MEM;
            goto out;
        }
    }

    *list = lp;

    if (len != NULL) {
        *len = ilen;
    }
out:
    if (err != AFC_E_SUCCESS && lp) {
        idevfs_list_free(lp);
    }

    if (info)
        afc_dictionary_free(info);

    if (real_path)
        free(real_path);

    return err;
}

int
split_appid_path(const char* arg, char** app_id, char** path)
{
    const char* path_part = NULL;

    if (arg == NULL || app_id == NULL || path == NULL)
        return -1;

    *path = *app_id = NULL;

    if ((path_part = strchr(arg, ':')) != NULL) {
        /* Split the path and path parts. */
        int app_id_len = path_part - arg;
        *path = make_docs_path(path_part + 1);
        if (*path == NULL)
            return -1;

        *app_id = calloc(app_id_len + 1, sizeof(char));
        if (*app_id == NULL) {
            free(*path);
            *path = NULL;
            return -1;
        }

        strncpy(*app_id, arg, app_id_len);
    } else {
        /*
         * No path portion. Just copy the app ID and use the "root" Documents
         * dir.
         */
        int app_id_len = strlen(arg);

        /*
         * the alloc is entirely unnecessary here, but makes it easier for the
         * caller (they can always free())
         */
        *app_id = calloc(strlen(arg) + 1, sizeof(char));
        if (*app_id == NULL)
            return -1;
        strncpy(*app_id, arg, app_id_len + 1);
        *path = make_docs_path("");
    }

    return 0;
}