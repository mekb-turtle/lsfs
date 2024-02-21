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
Usage: %s [-c] [-j] [-p] [-q] [filesystems...]\n"
"	-c --color --colour : adds color to the output\n"
"	-j --json : outputs in json\n"
"	-p --psuedofs : outputs psuedo filesystems too\n"
"	-q --quiet : only show mount and block usage on 1 line\n"
"	filesystems can either be the mount directory (e.g /), or the disk file (e.g /dev/sda1)\n"
"	omit filesystems to list all filesystems\n",
	argv0);
}

void escape_string(char *string) {
	// adds a backslash before quotes and backslashes in a string
	for (; *string; ++string) {
		if (*string == '"' || *string == '\\') putchar('\\');
		putchar(*string);
	}
}

void print_usage(bool color, fsblkcnt_t total, fsblkcnt_t free, fsblkcnt_t avail, unsigned long block) {
	fsblkcnt_t used = total - free;
	double percent = (double)used / (double)total * 100.0;
	char *s_used = display_bytes(used * block);
	char *s_total = display_bytes(total * block);
	char *s_free = display_bytes(free * block);
	char *s_avail = display_bytes(avail * block);
	if (color) {
		printf("" RESET COLOR"%s"RESET" used ("COLOR PERCENTAGE_FORMAT"%%"RESET"), "COLOR"%s"RESET" total, "COLOR"%s"RESET" free, "COLOR"%s"RESET" available",
			s_used, percent, s_total, s_free, s_avail);
	} else {
		printf("%s used ("PERCENTAGE_FORMAT"%%), %s total, %s free, %s available",
			s_used, percent, s_total, s_free, s_avail);
	}
}

int main(int argc, char *argv[]) {
#define INVALID { usage(argv[0]); return 1; }
	bool color_flag = 0;
	bool json_flag = 0;
	bool psuedofs_flag = 0;
	bool quiet_flag = 0;
	bool flag_done = 0;
	char* fs[argc];
	int fsc = 0;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] != '\0' && !flag_done) {
			if (argv[i][1] == '-' && argv[i][2] == '\0') flag_done = 1;
			else {
				if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0 || strcmp(argv[i], "--colour") == 0) {
					if (json_flag || color_flag) INVALID;
					color_flag = 1;
				} else
				if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
					if (json_flag || color_flag || quiet_flag) INVALID;
					json_flag = 1;
				} else
				if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
					if (json_flag || quiet_flag) INVALID;
					quiet_flag = 1;
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

	if (json_flag) printf("[");

	FILE *f = setmntent("/proc/self/mounts", "r");
	struct mntent *m;
	bool first = false;

	while ((m = getmntent(f))) {
		if (!psuedofs_flag) {
			if (m->mnt_fsname[0] != '/') continue;
			if (m->mnt_dir[0]    != '/') continue;
		}
		if (fsc > 0) {
			bool skip = true;
			for (int i = 0; i < fsc; ++i) {
				if (fs[i] == NULL) continue;
				if (strcmp(m->mnt_fsname, fs[i]) != 0
						&& strcmp(m->mnt_dir, fs[i]) != 0)
					continue;
				fs[i] = NULL;
				skip = false;
			}
			if (skip) continue;
		}
		struct statvfs vfs;
		if (statvfs(m->mnt_dir, &vfs) < 0) { fprintf(stderr, "statvfs: %s: %s\n", m->mnt_dir, strerror(errno)); } else if (vfs.f_blocks > 0) {
			if (json_flag) {
				if (first) printf(",");
				first = true;

				printf("{\"mnt\":{");

				printf("\"dir\":\""); escape_string(m->mnt_dir);
				printf("\",\"fsname\":\""); escape_string(m->mnt_fsname);
				printf("\",\"type\":\""); escape_string(m->mnt_type);
				printf("\",\"opts\":\""); escape_string(m->mnt_opts);
				printf("\",\"freq\":%i,", m->mnt_freq);
				printf("\"passno\":%i", m->mnt_passno);

				printf("},\"vfs\":{\"file\":");

				if (vfs.f_files) {
					printf("{");
					printf("\"total\":%li,", vfs.f_files);
					printf("\"free\":%li,", vfs.f_ffree);
					printf("\"avail\":%li,", vfs.f_favail);
					printf("\"used\":%li", vfs.f_files - vfs.f_ffree);
					printf("}");
				} else {
					printf("null");
				}

				printf(",\"block\":");

				if (vfs.f_bsize) {
					printf("{");
					printf("\"total\":%li,", vfs.f_bsize);
					printf("\"free\":%li,", vfs.f_bfree);
					printf("\"avail\":%li,", vfs.f_bavail);
					printf("\"used\":%li", vfs.f_blocks - vfs.f_bfree);
					printf("}");
				} else {
					printf("null");
				}

				printf("}}");
			} else {
				if (quiet_flag) {
					if (color_flag) {
						printf(RESET COLOR"%s"RESET" mounted at "COLOR"%s"RESET, m->mnt_fsname, m->mnt_dir);
					} else {
						printf("%s mounted at %s", m->mnt_fsname, m->mnt_dir);
					}
					printf(RESET ", ");
					if (vfs.f_blocks > 0) print_usage(color_flag, vfs.f_blocks, vfs.f_bfree, vfs.f_bavail, vfs.f_bsize);
					printf("\n");
				} else {
					if (color_flag) {
						printf(RESET COLOR"%s"RESET" mounted at "COLOR"%s"RESET"\n", m->mnt_fsname, m->mnt_dir);
						printf(RESET"type: "COLOR"%s"RESET", opts: "COLOR"%s"RESET"\n", m->mnt_type, m->mnt_opts);
					} else {
						printf("%s mounted at %s\n", m->mnt_fsname, m->mnt_dir);
						printf("type: %s, opts: %s\n", m->mnt_type, m->mnt_opts);
					}
					if (vfs.f_blocks > 0) {
						printf("block usage: ");
						print_usage(color_flag, vfs.f_blocks, vfs.f_bfree, vfs.f_bavail, vfs.f_bsize);
						printf("\n");
					}
					if (vfs.f_files > 0) {
						printf("files usage: ");
						print_usage(color_flag, vfs.f_files, vfs.f_ffree, vfs.f_favail, 1);
						printf("\n");
					}
					printf("\n");
				}
			}
		}
	}

	endmntent(f);

	if (json_flag) printf("]\n");

	bool fail = false;
	for (int i = 0; i < fsc; ++i) {
		if (fs[i] != NULL) {
			fprintf(stderr, "Filesystem %s not found\n", fs[i]);
			fail = true;
		}
	}
	if (fail) return 1;

	return 0;
}
