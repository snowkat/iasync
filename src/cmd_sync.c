#include "config.h"

#include "cmds.h"
#include "idevfs.h"
#include "log.h"
#include "strlist.h"
#include "sync_lock.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <getopt.h>

#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <fts.h>

/// @brief Structure for keeping track of what to do with given paths.
struct sync_tasks
{
    /// @brief Relative paths to copy to the iOS device.
    strlist_t to_copy;
    /// @brief List of relative paths to files specifically to ignore.
    strlist_t to_ignore;
    /// @brief Remote paths to delete from the iOS device.
    strlist_t to_delete;
    /// @brief List of relative directories to be created.
    strlist_t make_dirs;
};

static atomic_bool should_print_progress;

/*
 * Signal handler for timer events.
 */
static void
timer_handler(int sig, siginfo_t* si, void* uc)
{
    UNUSED(si);
    UNUSED(uc);

    atomic_store(&should_print_progress, true);

    signal(sig, SIG_IGN);
}

/// @brief Converts a `struct timespec` to `uint64_t`.
/// @return An unsigned 64-bit integer containing the timestamp, with
/// microsecond precision.
static inline uint64_t
tm2uint(struct timespec ts)
{
    /*
     * We're dealing with timespec structs (nsec), but iOS/AFC seems to only
     * have usec precision (backwards compatibility?). In most cases it's not
     * the end of the world, so we'll just lop off the last thousand.
     */
    uint64_t usec = (ts.tv_nsec / 1000);
    return (ts.tv_sec * 1000000000) + (usec * 1000);
}

/*
 * Figures out what files and folders need to be copied/deleted/ignored/etc.
 */
int
enumerate_sync(idevfs_t* idfs, char* local_dir, struct sync_tasks* tasks)
{
    char** raw_paths;
    strlist_t paths, dirs;
    afc_error_t err;
    int len, rroot_len, lroot_len;

    if (idfs == NULL || local_dir == NULL || tasks == NULL)
        return -1;

    atomic_init(&should_print_progress, false);

    tasks->to_copy = strlist_new(NULL, 0);
    tasks->to_ignore = strlist_new(NULL, 0);
    tasks->to_delete = strlist_new(NULL, 0);
    tasks->make_dirs = strlist_new(NULL, 0);
    ALLOC_ASSERT(tasks->to_copy);
    ALLOC_ASSERT(tasks->to_ignore);
    ALLOC_ASSERT(tasks->to_delete);
    ALLOC_ASSERT(tasks->make_dirs);

    /*
     * Get the length of the "root" dirs so we can parse out the subfolders
     * later.
     */
    rroot_len = strlen(idfs->cwd) + 1;
    lroot_len = strlen(local_dir) + 1;

    err = idevfs_readdir(idfs, "", &raw_paths, &len);
    if (err != AFC_E_SUCCESS) {
        log_printf(IA_ERROR, "Couldn't read root: %s\n", afc_strerror(err));
        return -1;
    }

    paths = strlist_new(raw_paths, len);
    ALLOC_ASSERT(paths);
    free(raw_paths);

    dirs = strlist_new(NULL, 0);
    ALLOC_ASSERT(dirs);

    /*
     * Iterate through every remote path first, and see which ones we care
     * about. This process is going to be sloowwwww, so try to accomplish as
     * much as possible at once.
     */
    for (size_t i = 0; i < paths->len; i++) {
        struct stat64 st;
        struct stat local_st;
        char* path = paths->begin[i];
        const char* fname;
        char* local_path;

        if (path == NULL)
            continue;

        fname = basename(path);

        if (fname[0] == '.') {
            /*
             * TODO: We skip all dotfiles so important metadata doesn't get
             * deleted. We should allow this with an option though.
             */
            continue;
#if 0
            // Skip "." and ".."
            if (fname[1] == '\0')
                continue;
            if (fname[1] == '.' && fname[2] == '\0')
                continue;
#endif
        }

        /*
         * First, check if we have this locally. If not, don't bother checking
         * anything else, just mark it to be deleted (or at least skip looking
         * further)
         */
        local_path = join_path(local_dir, path + rroot_len);
        ALLOC_ASSERT(local_path);
        if (lstat(local_path, &local_st) == -1) {
            if (errno == ENOENT) {
                /* Remote has the file but we don't. Add to delete list */
                char* full_path = strlist_claim(paths, i);
                char* rel_path = substr(full_path, rroot_len, -1);

                ALLOC_ASSERT(rel_path);
                strlist_push(tasks->to_delete, rel_path);

                free(full_path);
            } else {
                log_printf(IA_ERROR,
                           "Unable to stat local %s: %s\n",
                           local_path,
                           strerror(errno));
            }
            free(local_path);
            continue;
        }

        err = idevfs_stat(idfs, path, &st);
        if (err != AFC_E_SUCCESS) {
            log_printf(IA_ERROR,
                       "Unable to stat remote %s: %s\n",
                       path,
                       afc_strerror(err));
            continue;
        }

        free(local_path);

        char* rel_path = substr(path, rroot_len, -1);
        ALLOC_ASSERT(rel_path);

        if ((st.st_mode & S_IFDIR) != (local_st.st_mode & S_IFDIR)) {
            /*
             * If one side is a directory isn't, something's already wrong. Add
             * it to the delete list
             */
            strlist_push(tasks->to_delete, rel_path);
        } else if ((st.st_mode & S_IFDIR) != 0) {
            /* Another directory to traverse! */
            char** inner_paths;
            int inner_len;

            strlist_push(dirs, rel_path);

            err = idevfs_readdir(idfs, path, &inner_paths, &inner_len);
            if (err != AFC_E_SUCCESS) {
                log_printf(IA_ERROR,
                           "Unable to read remote dir %s: %s\n",
                           paths[i],
                           afc_strerror(err));
                continue;
            }

            /*
             * Copy the paths to the end of the list, so we can keep iterating.
             */
            strlist_pushall(paths, inner_paths, inner_len);

            /* !!! Only free the "outer" ptr, not the whole list !!! */
            free(inner_paths);
        } else {
            /* All other files, do our best to compare */
            if ((st.st_size != local_st.st_size) ||
                (tm2uint(st.st_mtim) != tm2uint(local_st.st_mtim))) {
                /* Not a match, overwrite */
                strlist_push(tasks->to_copy, rel_path);
            } else {
                /* File match! Add it to the ignore list */
                strlist_push(tasks->to_ignore, rel_path);
            }
        }
    }

    /*
     * Now we can iterate through the local filesystem. Since whatever the user
     * has is likely to be faster (and we have a lot more POSIX than AFC), let's
     * use fts(3).
     */
    {
        char* path_argv[] = { local_dir, NULL };
        FTS* fts = fts_open(path_argv, FTS_PHYSICAL, NULL);
        FTSENT* node = NULL;

        if (fts == NULL) {
            log_printf(IA_ERROR,
                       "Couldn't open local %s for traversing: %s\n",
                       local_dir,
                       strerror(errno));
            return -1;
        }

        while ((node = fts_read(fts)) != NULL) {
            char* rel_path;
            bool should_ignore = false;

            /* Skip the local root */
            if (node->fts_pathlen == (lroot_len - 1) &&
                !strncmp(local_dir, node->fts_path, lroot_len))
                continue;

            rel_path = substr(node->fts_path, lroot_len, node->fts_pathlen);
            ALLOC_ASSERT(rel_path);

            /* Should we ignore this one? */
            strlist_foreach(p, tasks->to_ignore)
            {
                if (strcmp(rel_path, p) == 0) {
                    should_ignore = true;
                    break;
                }
            }

            if (should_ignore) {
                free(rel_path);
                continue;
            }

            if ((node->fts_info & FTS_D) != 0) {
                /*
                 * If this directory doesn't exist on the device, we need to
                 * make it
                 */
                bool dir_exists = false;
                strlist_foreach(dir, dirs)
                {
                    if (strcmp(rel_path, dir) == 0) {
                        dir_exists = true;
                        break;
                    }
                }

                if (!dir_exists) {
                    strlist_push(tasks->make_dirs, rel_path);
                } else {
                    free(rel_path);
                }
            } else if ((node->fts_info & FTS_F) != 0) {
                /*
                 * Any files we're not ignoring, add to the list.
                 */
                strlist_push(tasks->to_copy, rel_path);
            } else {
                /*
                 * There shouldn't be any cases we care about that get us here
                 * (symlinks?)
                 */
                free(rel_path);
            }
        }

        fts_close(fts);
    }

    strlist_free(dirs);
    strlist_free(paths);

    return 0;
}

int
do_copy(idevfs_t* idfs, const char* from_path, const char* to_path)
{
    struct stat st;
    off_t flen = -1;
    int lfd, res = -1;
    afc_error_t err;
    uint64_t rfd = UINT64_MAX;
    size_t total_wr = 0;
    bool newline = false;

    err = afc_file_open(idfs->afc, to_path, AFC_FOPEN_WRONLY, &rfd);
    if (err != AFC_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't open remote %s: %s\n",
                   to_path,
                   afc_strerror(err));
        rfd = UINT64_MAX;
        goto out;
    }

    lfd = open(from_path, O_RDONLY);
    if (lfd == -1) {
        log_printf(
          IA_ERROR, "Couldn't open local %s: %s\n", from_path, strerror(errno));
        goto out;
    }

    if (fstat(lfd, &st) != 0) {
        log_printf(IA_ERROR,
                   "Unable to stat local file %s: %s\n",
                   from_path,
                   strerror(errno));

        /* Try to get file size manually */
        flen = lseek(lfd, 0, SEEK_END);
        lseek(lfd, 0, SEEK_SET);
    } else {
        flen = st.st_size;
    }

    while (true) {
        const size_t MAX_BLK_LEN = 4096 * 1024;
        char block[MAX_BLK_LEN];
        ssize_t blklen;

        blklen = read(lfd, block, MAX_BLK_LEN);
        if (blklen == 0) {
            /* Done! */
            res = 0;
            if (newline) {
                if (flen <= 0) {
                    printf("\r\x1b[2K  Wrote %zu/??? bytes\n", total_wr);
                } else {
                    float completeness = ((float)total_wr / (float)flen) * 100;
                    printf("\r\x1b[2K  Wrote %zu/%zu bytes (%.2f%%)\n",
                           total_wr,
                           flen,
                           completeness);
                }
            }
            break;
        } else if (blklen < 0) {
            log_printf(IA_ERROR,
                       "Error reading local %s: %s\n",
                       from_path,
                       strerror(errno));
            goto out;
        }
        for (size_t written = 0; written < (size_t)blklen;) {
            uint32_t wrlen;

            err =
              afc_file_write(idfs->afc, rfd, block, blklen - written, &wrlen);
            if (err != AFC_E_SUCCESS) {
                log_printf(IA_ERROR,
                           "Error writing to remote %s: %s\n",
                           to_path,
                           afc_strerror(err));
                goto out;
            }

            written += wrlen;
            total_wr += wrlen;
        }

        if (atomic_exchange(&should_print_progress, false)) {
            if (flen <= 0) {
                printf("\x1b[2K\r  Written %zu/??? bytes", total_wr);
            } else {
                float completeness = ((float)total_wr / (float)flen) * 100;
                printf("\x1b[2K\r  Written %zu/%zu bytes (%.2f%%)",
                       total_wr,
                       flen,
                       completeness);
            }
            fflush(stdout);
        }
        newline = true;
    }

out:
    if (rfd != UINT64_MAX) {
        afc_file_close(idfs->afc, rfd);
        if (res != 0) {
            /* If we had an open fd and failed, clean up the mess */
            afc_remove_path(idfs->afc, to_path);
        } else {
            err = afc_set_file_time(idfs->afc, to_path, tm2uint(st.st_mtim));
            if (err != AFC_E_SUCCESS) {
                log_printf(IA_ERROR,
                           "WARNING: Couldn't update mtime for %s!\n",
                           to_path);
            }
        }
    }

    if (lfd != -1)
        close(lfd);

    return res;
}

int
cmd_sync(int argc, char* argv[], globargs_t* ga)
{
    int ch, res = 0;
    char *app_id, *path;
    afc_error_t err;
    idevfs_t* idfs = NULL;
    struct sync_tasks tasks = { 0 };
    bool progress = false, dry_run = false, allow_delete = false,
         timer_set = true;
    timer_t timerid;
    struct sigaction sa;
    struct sync_lock lock = SYNCLOCK_INIT;

    struct option longopts[] = {
        { "allow-delete", no_argument, NULL, 'D' },
        { "dry-run",      no_argument, NULL, 'n' },
        { "progress",     no_argument, NULL, 'p' },
    };

    while ((ch = getopt_long(argc, argv, "Dnp", longopts, NULL)) != -1) {
        switch (ch) {
        case 'D':
            allow_delete = true;
            break;
        case 'n':
            dry_run = true;
            break;
        case 'p':
            progress = true;
            break;
        }
    }

    argc -= optind;
    argv += optind;

    // TODO: usage
    if (argc < 2)
        die("Bad arguments.\n");

    /* Truncate paths ending with '/' */
    {
        char* local = argv[0];
        int len = strlen(argv[0]);
        if (local[len - 1] == '/') {
            local[len - 1] = '\0';
        }
    }

    if (split_appid_path(argv[1], &app_id, &path) < 0) {
        die("Unable to parse provided remote path.\n");
        /* NOTREACHED */
    }
    if ((idfs = idevfs_setup(ga, app_id)) == NULL) {
        res = 2;
        goto done;
    }

    /* Announce our intent to begin the sync process */
    if (sync_lock_start(idfs, &lock) < 0) {
        res = 2;
        goto done;
    }

    log_printf(IA_INFO, ">> Enumerating files to sync. Please wait...\n");
    if (enumerate_sync(idfs, argv[0], &tasks) < 0) {
        log_printf(IA_ERROR, "Enumerating files for syncing failed!\n");
        res = 2;
        goto done;
    }

    if (dry_run) {
        strlist_foreach(dir, tasks.make_dirs)
        {
            printf("MKD %s\n", dir);
        }

        strlist_foreach(ent, tasks.to_delete)
        {
            printf("DEL %s\n", ent);
        }

        strlist_foreach(ent, tasks.to_copy)
        {
            printf("CPY %s\n", ent);
        }
        goto done;
    }

    log_printf(IA_INFO, ">> Syncing %zu files.\n", tasks.to_copy->len);

    /* Setup signal handler to print progress */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if ((sigaction(SIGUSR1, &sa, NULL) == -1)) {
        log_printf(IA_ERROR,
                   "Can't set signal handler? (%s) Continuing anyway...\n",
                   strerror(errno));
    } else if (progress) {
        struct sigevent sev;

        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGUSR1;
        sev.sigev_value.sival_ptr = &timerid;
        if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
            log_printf(
              IA_ERROR, "Can't setup progress timer: %s\n", strerror(errno));
        } else {
            struct itimerspec its;

            its.it_value.tv_sec = 1;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_sec = its.it_value.tv_sec;
            its.it_interval.tv_nsec = its.it_value.tv_nsec;

            if (timer_settime(timerid, 0, &its, NULL) == -1) {
                log_printf(IA_ERROR,
                           "Can't start progress timer: %s\n",
                           strerror(errno));
            } else {
                timer_set = true;
            }
        }
    }

    strlist_foreach(dir, tasks.make_dirs)
    {
        char* full_path = idevfs_canonpath(idfs, dir);
        ALLOC_ASSERT(full_path);
        log_printf(IA_DEBUG, "mkd %s\n", full_path);
        err = idevfs_mkpath(idfs, full_path);
        if (err != AFC_E_SUCCESS) {
            log_printf(IA_ERROR,
                       "Can't make remote dir %s: %s\n",
                       full_path,
                       afc_strerror(err));
        }
        free(full_path);
    }

    if (allow_delete) {
        strlist_foreach(ent, tasks.to_delete)
        {
            char* full_path = idevfs_canonpath(idfs, ent);
            ALLOC_ASSERT(full_path);
            log_printf(IA_DEBUG, "rm  %s\n", full_path);
            err = afc_remove_path_and_contents(idfs->afc, full_path);
            if (err != AFC_E_SUCCESS) {
                log_printf(
                  IA_ERROR, "Can't delete remote %s: %s\n", ent, afc_strerror);
            }
            free(full_path);
        }
    }

    strlist_foreach(ent, tasks.to_copy)
    {
        char* rem_path = idevfs_canonpath(idfs, ent);
        char* local_path = join_path(argv[0], ent);

        ALLOC_ASSERT(rem_path);
        ALLOC_ASSERT(local_path);

        log_printf(IA_VERBOSE, "%s\n", ent);
        do_copy(idfs, local_path, rem_path);

        free(local_path);
        free(rem_path);
    }
done:
    sync_lock_end(&lock);

    if (timer_set)
        timer_delete(timerid);
    if (tasks.make_dirs)
        strlist_free(tasks.make_dirs);
    if (tasks.to_copy)
        strlist_free(tasks.to_copy);
    if (tasks.to_delete)
        strlist_free(tasks.to_delete);
    if (tasks.to_ignore)
        strlist_free(tasks.to_ignore);
    if (idfs)
        idevfs_free(idfs);
    return res;
}