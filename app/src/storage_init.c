#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>
#include <string.h>

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

/*
int file_name_read(char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT]){
    
    int rc;
    struct fs_dir_t directory;
    struct fs_dirent entry;

    u_int8_t file_amount = 0;

    fs_dir_t_init(&directory);
    rc = fs_opendir(&directory, MOUNT_POINT);
    if (rc < 0) {
        printk("fs_opendir(%s) failed rc=%d\n", MOUNT_POINT, rc);
        return rc;
    }
    
    while(file_amount < MAX_FILE_AMOUNT){
        rc = fs_readdir(&directory, &entry);
        if (rc < 0) {
            printk("fs_readdir failed rc=%d\n", rc);
            break;
        }

        if (entry.name[0] == 0) {
            rc = 0;
            break;
        }

        if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) {
            continue;
        }

        if (entry.type != FS_DIR_ENTRY_FILE) {
            continue;
        }

        const char *file_extension = strrchr(entry.name, '.'); // looks for the file extension of the file
        
    }
}
    */