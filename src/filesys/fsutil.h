#ifndef FILESYS_FSUTIL_H
#define FILESYS_FSUTIL_H

int fsutil_ls(char **argv);
int fsutil_cat(char **argv);
int fsutil_rm(char **argv);
int fsutil_extract(char **argv);
int fsutil_append(char **argv);

#endif /**< filesys/fsutil.h */
