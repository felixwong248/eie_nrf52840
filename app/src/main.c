#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <errno.h>
#include <ff.h>

static FATFS fat_fs;

static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .mnt_point = "/SD:",
    .storage_dev = "SD",
};

static void ls_root(void)
{
    struct fs_dir_t dir;
    struct fs_dirent entry;
    int rc;

    fs_dir_t_init(&dir);

    rc = fs_opendir(&dir, "/SD:");
    printk("fs_opendir rc=%d\n", rc);
    if (rc != 0) {
        return;
    }

    while (1) {
        rc = fs_readdir(&dir, &entry);
        if (rc != 0) {
            printk("fs_readdir rc=%d\n", rc);
            break;
        }
        if (entry.name[0] == 0) {
            printk("(end of dir)\n");
            break;
        }

        printk("%c %s (%u bytes)\n",
               (entry.type == FS_DIR_ENTRY_DIR) ? 'D' : 'F',
               entry.name,
               entry.size);
    }

    fs_closedir(&dir);
}

int main(void)
{
    int rc;

    printk("\n--- SD test start ---\n");

    rc = disk_access_init("SD");
    printk("disk_access_init(\"SD\") rc=%d\n", rc);
    if (rc != 0) {
        printk("disk init failed; sleeping\n");
        while (1) k_sleep(K_SECONDS(2));
    }
    printk("About to mount...\n");
    rc = fs_mount(&mp);
    printk("Done mount...\n");
    
    printk("fs_mount rc=%d (%s)\n", rc,
           (rc == 0) ? "OK" :
           (rc == -EINVAL) ? "EINVAL" :
           (rc == -ENODEV) ? "ENODEV" :
           (rc == -EIO) ? "EIO" :
           "other");
    if (rc != 0) {
        printk("mount failed; sleeping\n");
        while (1) k_sleep(K_SECONDS(2));
    }

    printk("Mounted OK\n");
    ls_root();

    printk("Done listing; sleeping\n");
    while (1) {
        k_sleep(K_SECONDS(2));
    }
}
