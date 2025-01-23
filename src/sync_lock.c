#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "sync_lock.h"

#define SYNC_LOCK_FILE "/com.apple.itunes.lock_sync"

#define NP_POST(np, msg)                                                       \
    {                                                                          \
        np_error_t _np_err = np_post_notification((np), (msg));                \
        if (_np_err != NP_E_SUCCESS) {                                         \
            log_printf(IA_ERROR,                                               \
                       "Sending notification %s failed. Error %d\n",           \
                       (msg),                                                  \
                       _np_err);                                               \
            goto out;                                                          \
        }                                                                      \
    }

int
sync_lock_start(idevfs_t* idfs, struct sync_lock* lock)
{
    lockdownd_error_t ld_err;
    np_error_t np_err;
    afc_error_t afc_err;
    int res = -1;

    if (idfs == NULL)
        return -1;

    if (lock->np != NULL || lock->np_svc != NULL || lock->afc != NULL ||
        lock->afc_svc != NULL) {
        log_printf(IA_ERROR, "BUG: %s called more than once!\n", __func__);
        return -1;
    }

    ld_err =
      lockdownd_start_service(idfs->lockdown, NP_SERVICE_NAME, &(lock->np_svc));
    if (ld_err != LOCKDOWN_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't start sync notification service! Error %d\n",
                   ld_err);
        lock->np_svc = NULL;
        goto out;
    } else if (lock->np_svc == NULL) {
        log_printf(
          IA_ERROR,
          "Sync notification sharing service was started, but we weren't "
          "given access?\n");
        goto out;
    }

    np_err = np_client_new(idfs->dev, lock->np_svc, &(lock->np));
    if (np_err != NP_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't connect to sync notification service! Error %d\n",
                   np_err);
        goto out;
    }

    ld_err = lockdownd_start_service(
      idfs->lockdown, AFC_SERVICE_NAME, &(lock->afc_svc));
    if (ld_err != LOCKDOWN_E_SUCCESS) {
        log_printf(
          IA_ERROR,
          "Couldn't access AFC client for sync notifications! Error %d\n",
          ld_err);
        goto out;
    }

    /*
     * TODO: I guess we have to wait for AFC to start before we can start a
     * client? Why only this?
     */
    sleep(1);

    afc_err = afc_client_new(idfs->dev, lock->afc_svc, &(lock->afc));
    if (afc_err != AFC_E_SUCCESS) {
        log_printf(IA_ERROR,
                   "Couldn't connect to AFC client: %s\n",
                   afc_strerror(afc_err));
        goto out;
    }

    NP_POST(lock->np, NP_SYNC_WILL_START);

    NP_POST(lock->np, NP_SYNC_LOCK_REQUEST);

    afc_err =
      afc_file_open(lock->afc, SYNC_LOCK_FILE, AFC_FOPEN_RW, &(lock->lockfd));
    if (afc_err != AFC_E_SUCCESS) {
        log_printf(
          IA_ERROR, "Error getting sync lock: %s\n", afc_strerror(afc_err));
        goto out;
    }

    NP_POST(lock->np, NP_SYNC_DID_START);

    res = 0;
out:
    if (res != 0)
        sync_lock_end(lock);
    return res;
}

void
sync_lock_end(struct sync_lock* lock)
{

    if (lock == NULL)
        return;

    if (lock->lockfd != 0) {
        afc_file_close(lock->afc, lock->lockfd);
        lock->lockfd = -1;
    }

    if (lock->afc != NULL) {
        afc_client_free(lock->afc);
        lock->afc = NULL;
    }

    if (lock->afc_svc != NULL) {
        lockdownd_service_descriptor_free(lock->afc_svc);
        lock->afc_svc = NULL;
    }

    if (lock->np != NULL) {
        np_post_notification(lock->np, NP_SYNC_DID_FINISH);
        np_client_free(lock->np);
        lock->np = NULL;
    }

    if (lock->np_svc != NULL) {
        lockdownd_service_descriptor_free(lock->np_svc);
        lock->np_svc = NULL;
    }

    return;
}