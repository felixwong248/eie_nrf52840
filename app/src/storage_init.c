#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>

#include "storage_init.h"

static FATFS fat_fs;

static struct fs_mount_t mp = {
        .type = FS_FATFS,
        .fs_data = &fat_fs,
        .mnt_point = "/RAM:",
        .storage_dev = (void *)"RAM",   
    };

int storage_init(void)
{
    int rc;

    rc = disk_access_ioctl("RAM", DISK_IOCTL_CTRL_INIT, NULL);
    printk("disk init rc=%d\n", rc);
    if (rc != 0)
        return 0;

    // Format the disk as FAT (ONLY because this is RAM)
    rc = fs_mkfs(FS_FATFS, (uintptr_t)"RAM", NULL, 0);
    printk("fs_mkfs rc=%d\n", rc);
    if (rc < 0) 
        return rc;

    // Mount the filesystem
    rc = fs_mount(&mp);
    printk("fs_mount rc=%d\n", rc);
    if (rc != 0) 
        return rc;
    
    return 0;
}