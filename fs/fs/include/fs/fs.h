/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FS_H__
#define __FS_H__

#include <stddef.h>
#include <inttypes.h>

/*
 * Common interface to access files.
 */
struct fs_file;
struct fs_dir;
struct fs_dirent;

int fs_open(const char *filename, uint8_t access_flags, struct fs_file **);
int fs_close(struct fs_file *);
int fs_read(struct fs_file *, uint32_t len, void *out_data, uint32_t *out_len);
int fs_write(struct fs_file *, const void *data, int len);
int fs_seek(struct fs_file *, uint32_t offset);
uint32_t fs_getpos(const struct fs_file *);
int fs_filelen(const struct fs_file *, uint32_t *out_len);

int fs_unlink(const char *filename);
int fs_rename(const char *from, const char *to);
int fs_mkdir(const char *path);

int fs_opendir(const char *path, struct fs_dir **);
int fs_readdir(struct fs_dir *, struct fs_dirent **);
int fs_closedir(struct fs_dir *);
int fs_dirent_name(const struct fs_dirent *, size_t max_len,
  char *out_name, uint8_t *out_name_len);
int fs_dirent_is_dir(const struct fs_dirent *);

/*
 * File access flags.
 */
#define FS_ACCESS_READ          0x01
#define FS_ACCESS_WRITE         0x02
#define FS_ACCESS_APPEND        0x04
#define FS_ACCESS_TRUNCATE      0x08

/*
 * File access return codes.
 */
#define FS_EOK                  0       /* OK */
#define FS_ECORRUPT             1       /* Filesystem corrupt */
#define FS_HW_ERROR             2       /* Error access storage medium */
#define FS_ERANGE               3
#define FS_EINVAL               4
#define FS_ENOMEM               5       /* out of memory */
#define FS_ENOENT               6       /* no such file */
#define FS_EEMPTY               7
#define FS_EFULL                8
#define FS_EUNEXP               9
#define FS_EOS                  10
#define FS_EEXIST               11
#define FS_EACCESS              12
#define FS_EUNINIT              13

#endif