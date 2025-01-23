// sync_lock.h: Functions for holding an exclusive lock on sync

#ifndef __IASYNC_SYNC_LOCK_H
#define __IASYNC_SYNC_LOCK_H

#include "idevfs.h"
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/notification_proxy.h>

struct sync_lock
{
    afc_client_t afc;
    lockdownd_service_descriptor_t afc_svc;
    np_client_t np;
    lockdownd_service_descriptor_t np_svc;
    uint64_t lockfd;
};

#define SYNCLOCK_INIT                                                          \
    {                                                                          \
        NULL, NULL, NULL, NULL, -1,                                            \
    }

int
sync_lock_start(idevfs_t* idfs, struct sync_lock* lock);

void
sync_lock_end(struct sync_lock* lock);

#endif /* !__IASYNC_SYNC_LOCK_H */