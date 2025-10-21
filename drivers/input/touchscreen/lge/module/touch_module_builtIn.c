/* production_test.c
 *
 * Copyright (C) 2015 LGE.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/namei.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>

/*
 *  Include to touch core Header File
 */
#include <touch_hwif.h>
#include <touch_core.h>

#define MODULE_MAX_LOG_FILE_SIZE	(10 * 1024 * 1024) /* 10 M byte */
#define MODULE_MAX_LOG_FILE_COUNT	5
#define MODULE_FILE_STR_LEN		(128)

enum {
	TIME_INFO_SKIP,
	TIME_INFO_WRITE,
};

void module_write_file(struct device *dev, char *data, int write_time)
{
	struct file *file = NULL;
	char *fname = NULL;
	char time_string[64] = {0};
	struct timespec64 my_time = {0, };
	struct tm my_date = {0, };
	int boot_mode = TOUCH_NORMAL_BOOT;
	struct touch_core_data *ts;
	loff_t pos = 0;
	ssize_t ret = 0;

	TOUCH_TRACE();

	ts = (struct touch_core_data *) plist->touch_core_data;

	boot_mode = touch_check_boot_mode(ts->dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
		fname = "/data/vendor/touch/touch_module_self_test.txt";
		break;
	case TOUCH_MINIOS_AAT:
		fname = "/data/touch/touch_module_self_test.txt";
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		fname = "/data/touch/touch_module_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}


	if (fname) {
		file = filp_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
		return;
	}

	if (IS_ERR(file)) {
		TOUCH_E("%s : File open failed (err: %ld)\n", __func__, PTR_ERR(file));
		return;
	}

	if (write_time == TIME_INFO_WRITE) {
		ktime_get_real_ts64(&my_time);
		time64_to_tm(my_time.tv_sec,
				sys_tz.tz_minuteswest * 60 * (-1),
				&my_date);
		snprintf(time_string, 64,
			"\n[%02d-%02d %02d:%02d:%02d.%03lu]\n",
			my_date.tm_mon + 1,
			my_date.tm_mday, my_date.tm_hour,
			my_date.tm_min, my_date.tm_sec,
			(unsigned long) my_time.tv_nsec / 1000000);
		pos = file->f_pos;
		ret = kernel_write(file, time_string, strlen(time_string), &pos);
		file->f_pos = pos;
	}
	pos = file->f_pos;
	ret = kernel_write(file, data, strlen(data), &pos);
	file->f_pos = pos;

	filp_close(file, NULL);
}

void module_log_file_size_check(struct device *dev)
{
	char *fname = NULL;
	struct file *file = NULL;
	loff_t file_size = 0;
	int i = 0;
	char buf1[MODULE_FILE_STR_LEN] = {0};
	char buf2[MODULE_FILE_STR_LEN] = {0};
	int ret = 0;
	int boot_mode = TOUCH_NORMAL_BOOT;
	struct path path, old_path;
	struct dentry *old_dentry, *new_dentry;
	struct inode *dir_inode;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
		fname = "/data/vendor/touch/touch_module_self_test.txt";
		break;
	case TOUCH_MINIOS_AAT:
		fname = "/data/touch/touch_module_self_test.txt";
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		fname = "/data/touch/touch_module_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		file = filp_open(fname, O_RDONLY, 0666);
	} else {
		TOUCH_E("fname is NULL, can not open FILE\n");
		return;
	}

	if (IS_ERR(file)) {
		TOUCH_I("%s : ERR(%ld) Open file error [%s]\n",
				__func__, PTR_ERR(file), fname);
		return;
	}

	file_size = vfs_llseek(file, 0, SEEK_END);
	TOUCH_I("%s : [%s] file_size = %lld\n", __func__, fname, file_size);

	filp_close(file, NULL);

	if (file_size > MODULE_MAX_LOG_FILE_SIZE) {
		TOUCH_I("%s : [%s] file_size(%lld) > MAX_LOG_FILE_SIZE(%d)\n",
				__func__, fname, file_size, MODULE_MAX_LOG_FILE_SIZE);

		for (i = MODULE_MAX_LOG_FILE_COUNT - 1; i >= 0; i--) {
			if (i == 0)
				snprintf(buf1, sizeof(buf1), "%s", fname);
			else
				snprintf(buf1, sizeof(buf1), "%s.%d", fname, i);

			ret = kern_path(buf1, LOOKUP_FOLLOW, &path);

			if (ret == 0) {
				path_put(&path);
				TOUCH_I("%s : file [%s] exist\n", __func__, buf1);

				if (i == (MODULE_MAX_LOG_FILE_COUNT - 1)) {
					/* Delete the oldest file */
					ret = kern_path(buf1, 0, &old_path);
					if (ret == 0) {
						old_dentry = old_path.dentry;
						dir_inode = d_inode(old_dentry->d_parent);
						inode_lock(dir_inode);
						ret = vfs_unlink(mnt_user_ns(old_path.mnt), dir_inode, old_dentry, NULL);
						inode_unlock(dir_inode);
						path_put(&old_path);
						if (ret < 0) {
							TOUCH_E("failed to remove file [%s]\n", buf1);
						} else {
							TOUCH_I("%s : remove file [%s]\n", __func__, buf1);
						}
					}
				} else {
					/* Rename file */
					char *new_name;
					struct renamedata rd;
					snprintf(buf2, sizeof(buf2), "%s.%d", fname, (i + 1));
					new_name = strrchr(buf2, '/');
					if (new_name)
						new_name++;
					else
						new_name = buf2;
					
					ret = kern_path(buf1, 0, &old_path);
					if (ret == 0) {
						old_dentry = old_path.dentry;
						dir_inode = d_inode(old_dentry->d_parent);
						
						inode_lock(dir_inode);
						new_dentry = lookup_one_len(new_name, old_dentry->d_parent, strlen(new_name));
						if (!IS_ERR(new_dentry)) {
							memset(&rd, 0, sizeof(struct renamedata));
							rd.old_mnt_userns = mnt_user_ns(old_path.mnt);
							rd.old_dir = dir_inode;
							rd.old_dentry = old_dentry;
							rd.new_mnt_userns = mnt_user_ns(old_path.mnt);
							rd.new_dir = dir_inode;
							rd.new_dentry = new_dentry;
							rd.delegated_inode = NULL;
							rd.flags = 0;
							ret = vfs_rename(&rd);
							dput(new_dentry);
							if (ret == 0) {
								TOUCH_I("%s : rename file [%s] -> [%s]\n", __func__, buf1, buf2);
							} else {
								TOUCH_E("%s : failed to rename file [%s] -> [%s]\n", __func__, buf1, buf2);
							}
						}
						inode_unlock(dir_inode);
						path_put(&old_path);
					}
				}
			} else {
				TOUCH_I("%s : file [%s] does not exist (ret = %d)\n", __func__, buf1, ret);
			}
		}
	}
}
