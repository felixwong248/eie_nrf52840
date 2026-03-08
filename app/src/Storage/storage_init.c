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

    // intialize the sd card
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

// reads the files on the sd card and returns the number of files
int file_name_read(char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT])
{
    int rc;
    struct fs_dir_t directory;
    struct fs_dirent entry;

    uint8_t file_amount = 0;

    fs_dir_t_init(&directory);

    // fs_opendir opens the directory at /SD:
    rc = fs_opendir(&directory, MOUNT_POINT);
    if (rc < 0) {
        printk("fs_opendir(%s) failed rc=%d\n", MOUNT_POINT, rc);
        return rc;
    }

    while (file_amount < MAX_FILE_AMOUNT) {

        // fs_readdir reads one entry, and then gets stored in entry
        rc = fs_readdir(&directory, &entry);
        if (rc < 0) {
            printk("fs_readdir failed rc=%d\n", rc);
            break;
        }

        // checks if there are no more entries
        if (entry.name[0] == 0) {
            break;
        }

        // skips current and parent directory notation
        // . means the active folder where we currently are, and parent is one lvl above
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) {
            continue;
        }

        // skips anything that isnt a file
        if (entry.type != FS_DIR_ENTRY_FILE) {
            continue;
        }

        // copy file name into the array
        strncpy(file_names[file_amount], entry.name, MAX_LETTER_AMOUNT - 1);
        file_names[file_amount][MAX_LETTER_AMOUNT - 1] = '\0';
        file_amount++;
    }

    // close directory after all the files are read
    fs_closedir(&directory);

    return file_amount;
}

// creates the file path that the wav parser and streamer will point towards
// MOUNT_POINT is /SD:
// Add a "/"
// Add the file name 
// Then you get /SD:/Drive for example
void build_wav_path(char *dest, size_t dest_size, const char *filename)
{
    snprintf(dest, dest_size, "%s/%s", MOUNT_POINT, filename);
}