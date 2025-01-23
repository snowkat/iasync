// util.c: Utility functions
#include "config.h"

#include <libimobiledevice/lockdown.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmds.h"
#include "log.h"
#include "util.h"

char*
get_idevice_name(idevice_t device)
{
    lockdownd_client_t client = NULL;
    lockdownd_error_t err;
    char* name = NULL;

    if (device == NULL)
        return NULL;

    err = lockdownd_client_new_with_handshake(device, &client, PROJECT_NAME);
    if (err != LOCKDOWN_E_SUCCESS) {
        return NULL;
    }

    lockdownd_get_device_name(client, &name);
    lockdownd_client_free(client);

    return name;
}

idevice_t
get_idevice_by_globargs(globargs_t* ga)
{
    idevice_error_t err;
    idevice_t dev;
    enum idevice_options opts = IDEVICE_LOOKUP_NETWORK | IDEVICE_LOOKUP_USBMUX;

    if (ga == NULL)
        return NULL;

    if (ga->name != NULL) {
        /*
         * The only way to find a device by name is to iterate through every
         * UDID, pair with it, and see if it's the one we want. Not pretty, but
         * I'm taking the assumption most people don't have a dozen iOS devices
         * paired with their computer...
         */
        char** udids = NULL;
        int count;

        err = idevice_get_device_list(&udids, &count);
        if (err != IDEVICE_E_SUCCESS) {
            log_printf(IA_ERROR,
                       "Couldn't get device list! (is usbmux running?)\n");
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            char* name;
            if (idevice_new_with_options(&dev, udids[i], opts) !=
                IDEVICE_E_SUCCESS)
                continue;
            name = get_idevice_name(dev);
            if (name != NULL) {
                if (!strcmp(name, ga->name)) {
                    free(name);
                    break;
                } else {
                    // Not our device
                    free(name);
                    idevice_free(dev);
                    dev = NULL;
                }
            }
        }
    } else {
        if (idevice_new_with_options(&dev, ga->udid, opts) !=
            IDEVICE_E_SUCCESS) {
            return NULL;
        }
    }

    return dev;
}

char*
join_path(const char* p1, const char* p2)
{
    char* ccat;
    size_t ccat_len;
    if (p1 == NULL || p2 == NULL)
        return NULL;

    ccat_len = strlen(p1) + strlen(p2) + 2;

    ccat = calloc(ccat_len, sizeof(char));
    if (ccat == NULL)
        return NULL;

    snprintf(ccat, ccat_len, "%s/%s", p1, p2);

    return ccat;
}

/**
 * @brief Provides an allocated version of the new substring.
 *
 * @param root The main string to splice.
 * @param begin The index to begin the splice.
 * @param end The index to end the splice, or -1 to go until the string ends.
 * @return An allocated string representing the substring.
 */
char*
substr(const char* root, int begin, int end)
{
    char* result;
    int len;
    if (root == NULL)
        return NULL;
    begin = MAX(begin, 0);

    if (end < 0) {
        end = strlen(root);
    }

    /*
     * XXX: If end > strlen(root), this could be an issue!! But we provide
     * length in case a string is potentially not NULL-terminated (fts_read?),
     * so we'll have to trust our math.
     */

    if (end < begin) {
        log_printf(IA_ERROR, "substr: end (%d) < begin (%d)?\n", end, begin);
        return NULL;
    }

    /* Includes null terminator */
    len = end - begin + 1;
    result = calloc(len, sizeof(char));
    ALLOC_ASSERT(result);

    strncpy(result, root + begin, len);

    return result;
}