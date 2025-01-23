// cmd_lsdev.c: Lists available devices.
#include "config.h"

#include "cmds.h"
#include "log.h"
#include "util.h"

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct device_row
{
    char* udid;
    const char* type;
    char* name;
};

/*
 * Format string used for the table format.
 *   Columns, in order, are ["Name", "Conn.", and "UDID"]
 */
#define DEVICE_TABLE_ROW "%-*s   %-8s   %s\n"

int
cmd_lsdev(int argc, char* argv[], globargs_t* ga)
{
    idevice_info_t* devices = NULL;
    idevice_error_t err;
    struct device_row* rows = NULL;
    int name_max = 0, udid_max = 0, ch, count;
    bool print_hdrs = true;

    // None of the global args are useful to us
    UNUSED(ga);

    struct option longopts[] = {
        { "no-headers", no_argument, NULL, 'h' },
        { NULL,         0,           NULL, 0   }
    };

    while ((ch = getopt_long(argc, argv, "n", longopts, NULL)) != -1) {
        switch (ch) {
        case 'n':
            print_hdrs = false;
            break;
        }
    }

    err = idevice_get_device_list_extended(&devices, &count);

    if (err != IDEVICE_E_SUCCESS) {
        die("Couldn't get device list! (is usbmuxd running?)\n");
        /* NOTREACHED */
    }

    /* Don't bother allocating if we have no rows */
    if (count > 0) {
        rows = calloc(count, sizeof(struct device_row));
        if (rows == NULL) {
            perror("memory allocation error");
            idevice_device_list_extended_free(devices);
            return 2;
        }
    }

    /* make the table rows */
    for (int i = 0; i < count; i++) {
        int is_usb = (devices[i]->conn_type == CONNECTION_USBMUXD);
        idevice_t device = NULL;

        rows[i].type = is_usb ? "  USB  " : "Network";
        rows[i].udid = devices[i]->udid;
        udid_max = MAX(udid_max, (int)strlen(rows[i].udid));

        /* Open the device proper to get more info */
        err = idevice_new_with_options(&device,
                                       devices[i]->udid,
                                       is_usb ? IDEVICE_LOOKUP_USBMUX
                                              : IDEVICE_LOOKUP_NETWORK);

        if (err == IDEVICE_E_SUCCESS) {
            rows[i].name = get_idevice_name(device);
            idevice_free(device);

            if (rows[i].name != NULL)
                name_max = MAX(name_max, (int)strlen(rows[i].name));
        }
    }

    /* and once more, for stdout */
    if (print_hdrs)
        printf(DEVICE_TABLE_ROW, name_max, "Name", "Conn.", "UDID");
    for (int i = 0; i < count; i++) {
        printf(DEVICE_TABLE_ROW,
               name_max,
               STRING_OR_UNKNOWN(rows[i].name),
               rows[i].type,
               STRING_OR_UNKNOWN(rows[i].udid));
        if (rows[i].name != NULL)
            free(rows[i].name);
    }

    idevice_device_list_extended_free(devices);

    if (rows != NULL)
        free(rows);

    return 0;
}