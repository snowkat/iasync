// cmd_ls.c: List files for a given application
#include "config.h"

#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/libimobiledevice.h>

#include "cmds.h"
#include "idevfs.h"
#include "log.h"
#include "util.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
cmd_ls(int argc, char* argv[], globargs_t* ga)
{
    int res = 0, ch;
    afc_error_t afc_err;
    idevfs_t* idfs = NULL;
    char** filents = NULL;
    char** curent = NULL;
    char *app_id, *ls_path;
    bool list_all = false;

    struct option longopts[] = {
        { "all", no_argument, NULL, 'a' },
        { NULL,  0,           NULL, 0   },
    };

    while ((ch = getopt_long(argc, argv, "a", longopts, NULL)) != -1) {
        switch (ch) {
        case 'a':
            list_all = true;
            break;
        default:
            return -1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        die("App Bundle ID not provided.\n");

    if (split_appid_path(argv[0], &app_id, &ls_path) < 0) {
        die("Unable to parse provided remote path.\n");
        /* NOTREACHED */
    }

    if ((idfs = idevfs_setup(ga, app_id)) == NULL) {
        res = 2;
        goto done;
    }

    afc_err = afc_read_directory(idfs->afc, ls_path, &filents);
    if (afc_err != AFC_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't read '%s' (app %s, error %d)\n",
                   ls_path,
                   app_id,
                   afc_err);
        res = 2;
        goto done;
    }

    curent = filents;
    while (*curent != NULL) {
        if (list_all || *curent[0] != '.')
            printf("%s\n", *curent);
        curent++;
    }

done:
    if (filents)
        afc_dictionary_free(filents);
    if (idfs)
        idevfs_free(idfs);
    return res;
}