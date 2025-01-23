#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include "cmds.h"
#include "log.h"

typedef struct _cmd_meta
{
    const char* name;
    const char* desc;
    const char* usage_text;
    cmd_func_t fn;
} cmd_meta_t;

static cmd_meta_t cmd_meta[] = {
    { .name = "lsdevs", .usage_text = "[-n | --no-headers]",        .fn = cmd_lsdev  },
    { .name = "lsapps", .usage_text = "[-n | --no-headers]",        .fn = cmd_lsapps },
    { .name = "ls",     .usage_text = "[-a | --all] app_id[:path]", .fn = cmd_ls     },
    { .name = "sync",
     .usage_text = "[-DNP] local_path app_id[:path]",
     .fn = cmd_sync                                                                  },
};
const int cmd_count = (sizeof(cmd_meta) / sizeof(cmd_meta_t));

void
usage(const char* err)
{
    int i;
    int res = 1;

    if (err != NULL) {
        log_printf(IA_CRITICAL, "%s: %s\n", getprogname(), err);
        res = 2;
    }

    fprintf(stderr,
            "Usage: %s [-vq] [-n name | -u udid] command\n"
            "Commands:\n",
            getprogname());

    for (i = 0; i < cmd_count; i++) {
        fprintf(stderr, "\t%s", cmd_meta[i].name);
        if (cmd_meta[i].usage_text) {
            fprintf(stderr, " %s", cmd_meta[i].usage_text);
        }
        fprintf(stderr, "\n");
    }

    exit(res);
}

int
main(int argc, char* argv[])
{
    int i, ch;
    globargs_t ga = { 0 };
    enum log_level verbosity = IA_INFO;
    bool q_set = false, v_set = false;

    setprogname(argv[0]);

    struct option longopts[] = {
        { "name",    required_argument, NULL, 'n' },
        { "udid",    required_argument, NULL, 'u' },
        { "verbose", no_argument,       NULL, 'v' },
        { "quiet",   no_argument,       NULL, 'q' },
        { "help",    no_argument,       NULL, 'h' },
        { NULL,      0,                 NULL, 0   }
    };

    while ((ch = getopt_long(argc, argv, "+vqn:u:h", longopts, NULL)) != -1) {
        switch (ch) {
        case 'n':
            if (ga.udid != NULL) {
                usage("Only device UDID or name should be set.");
            }
            ga.name = optarg;
            break;
        case 'u':
            if (ga.name != NULL) {
                usage("Only device UDID or name should be set.");
            }
            ga.udid = optarg;
            break;
        case 'v':
            if (q_set)
                usage("-q and -v are mutually exclusive.");
            if (verbosity < IA_TRACE)
                verbosity++;
            v_set = true;
            break;
        case 'q':
            if (v_set)
                usage("-q and -v are mutually exclusive.");
            if (verbosity > IA_CRITICAL)
                verbosity--;
            q_set = true;
            break;
        case '?':
        default:
            usage(NULL);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage(NULL);
    }

    set_log_level(verbosity);

    for (i = 0; i < cmd_count; i++) {
        if (!strcmp(cmd_meta[i].name, argv[0])) {
            int res = (cmd_meta[i].fn)(argc, argv, &ga);
            if (res == -1)
                usage(NULL);
            else
                return res;
        }
    }

    usage("Unknown command.");
    /* NOTREACHED */
}
