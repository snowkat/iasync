// cmd_apps.c: Lists available apps for a given device.

#include "config.h"

#include "cmds.h"
#include "log.h"
#include "util.h"

#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Format string used for the table format.
 * Columns, in order, are ["Name", "Bundle ID", and "Supports File Sharing"]
 */
#define APP_TABLE_ROW "%-*s   %-*s\n"

static char*
plist_dict_item_val(plist_t node, const char* key)
{
    plist_t item_node;
    char* val;
    if (node == NULL || key == NULL)
        return NULL;
    if (plist_get_node_type(node) != PLIST_DICT)
        return NULL;

    item_node = plist_dict_get_item(node, key);
    if (item_node == NULL || plist_get_node_type(item_node) != PLIST_STRING)
        return NULL;
    plist_get_string_val(item_node, &val);

    return val;
}

struct app_row
{
    char* disp_name;
    char* bundle_id;
    int sharing_enabled;
};

int
cmd_lsapps(int argc, char* argv[], globargs_t* ga)
{
    idevice_t dev = get_idevice_by_globargs(ga);
    instproxy_client_t proxy_client = NULL;
    instproxy_error_t err;
    plist_t apps, opts;
    struct app_row* rows = NULL;
    size_t count;
    int bundle_max = 0, name_max = 0, ch;
    bool print_hdrs = true;

    struct option longopts[] = {
        { "no-headers", no_argument, NULL, 'h' },
        { NULL,         0,           NULL, 0   }
    };

    while ((ch = getopt_long(argc, argv, "n", longopts, NULL)) != -1) {
        switch (ch) {
        case 'n':
            print_hdrs = false;
            break;
        default:
            return -1;
        }
    }

    if (dev == NULL)
        die("Couldn't find device!\n");

    err = instproxy_client_start_service(dev, &proxy_client, PROJECT_NAME);
    if (err != INSTPROXY_E_SUCCESS) {
        idevice_free(dev);
        die("Installation proxy connection failed (error %d)\n", err);
        /* NOTREACHED */
    }

    opts = instproxy_client_options_new();
    instproxy_client_options_add(opts, "ApplicationType", "Any", NULL);
    instproxy_client_options_set_return_attributes(opts,
                                                   "CFBundleDisplayName",
                                                   "CFBundleIdentifier",
                                                   "UIFileSharingEnabled",
                                                   NULL);
    err = instproxy_browse(proxy_client, opts, &apps);
    instproxy_client_options_free(opts);
    if (err != INSTPROXY_E_SUCCESS) {
        die("Unable to get programs from installation proxy (error %d)\n", err);
        /* NOTREACHED */
    }

    count = plist_array_get_size(apps);

    if (count > 0) {
        rows = calloc(count, sizeof(struct app_row));
        if (rows == NULL) {
            perror("memory allocation error");
            plist_free(apps);
            instproxy_client_free(proxy_client);
            return 2;
        }
    }

    for (size_t i = 0; i < count; i++) {
        plist_t cur_app = plist_array_get_item(apps, i);
        rows[i].bundle_id = plist_dict_item_val(cur_app, "CFBundleIdentifier");
        if (rows[i].bundle_id != NULL)
            bundle_max = MAX(bundle_max, (int)strlen(rows[i].bundle_id));

        rows[i].disp_name = plist_dict_item_val(cur_app, "CFBundleDisplayName");
        if (rows[i].disp_name != NULL)
            name_max = MAX(name_max, (int)strlen(rows[i].disp_name));

        rows[i].sharing_enabled =
          plist_dict_get_bool(cur_app, "UIFileSharingEnabled");
    }

    if (print_hdrs)
        printf(APP_TABLE_ROW, name_max, "App Name", bundle_max, "Bundle ID");
    for (size_t i = 0; i < count; i++) {
        if (!rows[i].sharing_enabled)
            continue;
        printf(APP_TABLE_ROW,
               name_max,
               STRING_OR_UNKNOWN(rows[i].disp_name),
               bundle_max,
               STRING_OR_UNKNOWN(rows[i].bundle_id));
    }

    plist_free(apps);
    instproxy_client_free(proxy_client);
    idevice_free(dev);

    return 0;
}