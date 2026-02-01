#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>

#include "storage_init.h"

#define DISK_NAME   "SD"
#define MOUNT_POINT "/SD:"

static FATFS fat_fs;

static struct fs_mount_t mp = {
        .type = FS_FATFS,
        .fs_data = &fat_fs,
        .mnt_point = MOUNT_POINT,
        .storage_dev = (void *)DISK_NAME,   
    };

int storage_init(void)
{
    int rc;

    rc = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
    printk("disk init rc=%d\n", rc);
    if (rc != 0){
        return rc;
    }

    // Mount the filesystem
    rc = fs_mount(&mp);
    printk("fs_mount rc=%d\n", rc);
    if (rc != 0){
        return rc;
    }
    return 0;
}