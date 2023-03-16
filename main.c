#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <mntent.h>
#include <errno.h>
#define COLOR "\x1b[38;5;14m"
#define RESET "\x1b[0m"
#define BYTES_PADDING 100
#define BYTES_FORMAT "%.2f"
#define PERCENTAGE_FORMAT "%.2f"
char *display_bytes(unsigned long bytes) {
	if (bytes == 0) return "0";
	bytes *= BYTES_PADDING; // allow for extra decimal digits
	uint8_t suffixI = 0;
	while (bytes >= 1024 * BYTES_PADDING) {
		bytes /= 1024; // divide by 1024 and use the next suffix
		++suffixI;
	}
	double bytes_dec = 0;
	if (suffixI > 0) bytes_dec = (double)(bytes % BYTES_PADDING) / BYTES_PADDING; // the extra decimal digits
	bytes /= BYTES_PADDING; // bytes is actual byte count now
	char *temp_dec;
	char *temp_zero_dec;
	if (suffixI > 0) {
		temp_dec = malloc(8);
		sprintf(temp_dec, BYTES_FORMAT, bytes_dec); // use temp string so we can remove the 0 before the decimal point, 6.5G would look like 60.5G otherwise
		temp_zero_dec = strchr(temp_dec, '.'); // find dot in the string
	}
	char *str = malloc(24); // final string we return
	const char *suffixes = "KMGTPEZY";
	if (suffixI > 0) {
		sprintf(str, "%li%s%c", bytes, temp_zero_dec, suffixes[suffixI-1]);
	} else {
		sprintf(str, "%li", bytes); // don't put the temp string if there's no dot
	}
	if (suffixI > 0)
		free(temp_dec);
	return str;
}
void usage(char *argv0) {
	fprintf(stderr, "\
Usage: %s [-c] [-s] [-p] [-e] [filesystems...]\n"
"	-c --color --colour : adds color to the output\n"
"	-s --script : outputs in a way parseable by scripts\n"
"	-p --psuedofs : outputs psuedo filesystems too\n"
"	filesystems can either be the mount directory (e.g /), or the disk file (e.g /dev/sda1)\n"
"	omit filesystems to list all filesystems\n",
	argv0);
}
int main(int argc, char *argv[]) {
#define INVALID { usage(argv[0]); return 1; }
	bool color_flag = 0;
	bool script_flag = 0;
	bool psuedofs_flag = 0;
	bool flag_done = 0;
	char* fs[argc];
	int fsc = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] != '\0' && !flag_done) {
			if (argv[i][1] == '-' && argv[i][2] == '\0') flag_done = 1;
			else {
				if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0 || strcmp(argv[i], "--colour") == 0) {
					if (script_flag || color_flag) INVALID;
					color_flag = 1;
				} else
				if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--script") == 0) {
					if (script_flag || color_flag) INVALID;
					script_flag = 1;
				} else
				if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--psuedofs") == 0) {
					if (psuedofs_flag) INVALID;
					psuedofs_flag = 1;
				} else
				INVALID;
			}
		} else {
			fs[fsc++] = argv[i];
		}
	}
	FILE *f = setmntent("/proc/self/mounts", "r");
	struct mntent *m;
	int idx = 0;
	while ((m = getmntent(f))) {
		if (!psuedofs_flag) {
			if (m->mnt_fsname[0] != '/') continue;
			if (m->mnt_dir[0]    != '/') continue;
		}
		for (int i = 0; i < fsc; ++i) {
			if (strcmp(m->mnt_fsname, fs[i]) != 0)
			if (strcmp(m->mnt_dir, fs[i]) != 0)
				continue;
			fs[i] = NULL;
		}
		struct statvfs vfs;
		if (statvfs(m->mnt_dir, &vfs) < 0) { fprintf(stderr, "statvfs: %s: %s\n", m->mnt_dir, strerror(errno)); } else if (vfs.f_blocks > 0) {
			if (script_flag) {
				printf("%idir=%s\n", idx, m->mnt_dir);
				printf("%ifsname=%s\n", idx, m->mnt_fsname);
				printf("%itype=%s\n", idx, m->mnt_type);
				printf("%iopts=%i\n", idx, m->mnt_opts);
				printf("%ifreq=%i\n", idx, m->mnt_freq);
				printf("%ipassno=%s\n", idx, m->mnt_passno);
				printf("%iinode=%li\n", idx, vfs.f_files);
				printf("%iinodefree=%li\n", idx, vfs.f_ffree);
				printf("%iinodeavail=%li\n", idx, vfs.f_favail);
				printf("%iinodeused=%li\n", idx, vfs.f_files - vfs.f_ffree);
				printf("%iblock=%li\n", idx, vfs.f_blocks);
				printf("%iblockfree=%li\n", idx, vfs.f_bfree);
				printf("%iblockavail=%li\n", idx, vfs.f_bavail);
				printf("%iblockused=%li\n", idx, vfs.f_blocks - vfs.f_bfree);
				++idx;
			} else {
				char *block_used = display_bytes((vfs.f_blocks-vfs.f_bfree)*vfs.f_bsize);
				char *block = display_bytes(vfs.f_blocks*vfs.f_bsize);
				bool block_is = false;
				double block_percentage;
				if (vfs.f_blocks > 0) {
					block_percentage = 100.0-(((double)vfs.f_bfree*100.0)/(double)vfs.f_blocks);
					block_is = true;
				}
				char *inode_used = display_bytes(vfs.f_files-vfs.f_ffree);
				char *inode = display_bytes(vfs.f_files);
				bool inode_is = false;
				double inode_percentage;
				if (vfs.f_files > 0) {
					inode_percentage = 100.0-(((double)vfs.f_ffree*100.0)/(double)vfs.f_files);
					inode_is = true;
				}
				if (color_flag) {
					printf(RESET COLOR"%s"RESET" mounted at "COLOR"%s"RESET"\n", m->mnt_fsname, m->mnt_dir);
					printf(RESET"type: "COLOR"%s"RESET", opts: "COLOR"%s"RESET"\n", m->mnt_type, m->mnt_opts);
					if (block_is)
						printf(RESET"block usage: "COLOR"%s"RESET"/"COLOR"%s"RESET" ("COLOR PERCENTAGE_FORMAT"%%"RESET")\n", block_used, block, block_percentage);
					if (inode_is)
						printf(RESET"inode usage: "COLOR"%s"RESET"/"COLOR"%s"RESET" ("COLOR PERCENTAGE_FORMAT"%%"RESET")\n", inode_used, inode, inode_percentage);
					printf("\n");
				} else {
					printf("%s mounted at %s\n", m->mnt_fsname, m->mnt_dir);
					printf("type: %s, opts: %s\n", m->mnt_type, m->mnt_opts);
					if (block_is)
						printf("block usage: %s/%s ("PERCENTAGE_FORMAT"%%)\n", block_used, block, block_percentage);
					if (inode_is)
						printf("inode usage: %s/%s ("PERCENTAGE_FORMAT"%%)\n", inode_used, inode, inode_percentage);
					printf("\n");
				}
			}
		}
	}
	endmntent(f);
	bool fail = false;
	for (int i = 0; i < fsc; ++i) {
		if (fs[i] != NULL) {
			fprintf(stderr, "Filesystem %s not found", fs[i]);
			fail = true;
		}
	}
	if (fail) return 1;
	return 0;
}
