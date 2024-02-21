#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <mntent.h>
#include <getopt.h>
#include <err.h>

#define COLOR "\x1b[38;5;14m"
#define RESET "\x1b[0m"
#define BYTES_PADDING 100
#define BYTES_FORMAT "%.2f"
#define PERCENTAGE_FORMAT "%.2f"

static struct option options_getopt[] = {
        {"help",     no_argument, 0, 'h'},
        {"version",  no_argument, 0, 'V'},
        {"colour",   no_argument, 0, 'c'},
        {"color",    no_argument, 0, 'c'},
        {"json",     no_argument, 0, 'j'},
        {"psuedofs", no_argument, 0, 'p'},
        {"quiet",    no_argument, 0, 'q'},
        {0,          0,           0, 0  }
};

char *display_bytes(unsigned long bytes) {
	if (bytes == 0) return "0";

	bytes *= BYTES_PADDING; // allow for extra decimal digits
	uint8_t suffixI = 0;
	while (bytes >= 1024 * BYTES_PADDING) {
		bytes /= 1024; // divide by 1024 and use the next suffix
		++suffixI;
	}

	double bytes_dec = 0;
	if (suffixI > 0) bytes_dec = (double) (bytes % BYTES_PADDING) / BYTES_PADDING; // the extra decimal digits
	bytes /= BYTES_PADDING;                                                        // bytes is actual byte count now
	char *temp_dec;
	char *temp_zero_dec;
	if (suffixI > 0) {
		temp_dec = malloc(8);
		sprintf(temp_dec, BYTES_FORMAT, bytes_dec); // use temp string so we can remove the 0 before the decimal point, 6.5G would look like 60.5G otherwise
		temp_zero_dec = strchr(temp_dec, '.');      // find dot in the string
	}

	char *str = malloc(24); // final string we return

	const char *suffixes = "KMGTPEZY";
	if (suffixI > 0) {
		sprintf(str, "%li%s%c", bytes, temp_zero_dec, suffixes[suffixI - 1]);
	} else {
		sprintf(str, "%li", bytes); // don't put the temp string if there's no dot
	}

	if (suffixI > 0)
		free(temp_dec);

	return str;
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
	double percent = (double) used / (double) total * 100.0;
	char *s_used = display_bytes(used * block);
	char *s_total = display_bytes(total * block);
	char *s_free = display_bytes(free * block);
	char *s_avail = display_bytes(avail * block);
	if (color) {
		printf("" RESET COLOR "%s" RESET " used (" COLOR PERCENTAGE_FORMAT "%%" RESET "), " COLOR "%s" RESET " total, " COLOR "%s" RESET " free, " COLOR "%s" RESET " available",
		       s_used, percent, s_total, s_free, s_avail);
	} else {
		printf("%s used (" PERCENTAGE_FORMAT "%%), %s total, %s free, %s available",
		       s_used, percent, s_total, s_free, s_avail);
	}
}

int main(int argc, char *argv[]) {
	bool invalid = false;
	int opt;

	bool color_flag = 0;
	bool json_flag = 0;
	bool psuedofs_flag = 0;
	bool quiet_flag = 0;

	// argument handling
	while ((opt = getopt_long(argc, argv, ":hVcjpq", options_getopt, NULL)) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [-c] [-j] [-p] [-q] [filesystems...]\n"
				       "-h --help: Shows help text\n"
				       "-V --version: Shows the version\n"
				       "-c --color --colour: adds color to the output\n"
				       "-j --json: outputs in json\n"
				       "-p --psuedofs: outputs psuedo filesystems too\n"
				       "-q --quiet: only show mount and block usage on 1 line\n"
				       "filesystems can either be the mount directory (e.g /), or the disk file (e.g /dev/sda1)\n"
				       "omit filesystems to list all filesystems\n",
				       TARGET);
				return 0;
			case 'V':
				printf("%s %s\n", TARGET, VERSION);
				return 0;
			default:
				if (!invalid) {
					switch (opt) {
						case 'c':
							if (json_flag || color_flag) invalid = true;
							else
								color_flag = 1;
							break;
						case 'j':
							if (json_flag || color_flag || quiet_flag) invalid = true;
							else
								json_flag = 1;
							break;
						case 'q':
							if (json_flag || quiet_flag) invalid = true;
							else
								quiet_flag = 1;
							break;
						case 'p':
							if (psuedofs_flag) invalid = true;
							else
								psuedofs_flag = 1;
							break;
						default:
							invalid = true;
							break;
					}
				}
				break;
		}
	}

	if (invalid || argc < optind)
		errx(1, "Invalid usage, try --help");

	FILE *f = setmntent("/proc/self/mounts", "r");
	struct mntent *m;
	bool first = false;

	size_t fs_count = 0;
	char *fs[argc - optind];

	for (size_t i = 0, j = optind; j < argc; ++i, ++j, ++fs_count) {
		fs[i] = argv[j];
	}

	if (json_flag) printf("[");

	while ((m = getmntent(f))) {
		if (!psuedofs_flag) {
			if (m->mnt_fsname[0] != '/') continue;
			if (m->mnt_dir[0] != '/') continue;
		}
		if (fs_count) {
			bool skip = true;
			for (size_t i = 0; i < fs_count; ++i) {
				if (!fs[i]) continue;
				if (strcmp(m->mnt_fsname, fs[i]) != 0 && strcmp(m->mnt_dir, fs[i]) != 0)
					continue;
				skip = false;
				fs[i] = NULL;
			}
			if (skip) continue;
		}
		struct statvfs vfs;
		if (statvfs(m->mnt_dir, &vfs) < 0) {
			err(1, "statvfs");
		} else if (vfs.f_blocks > 0) {
			if (json_flag) {
				if (first) printf(",");
				first = true;

				printf("{\"mnt\":{");

				printf("\"dir\":\"");
				escape_string(m->mnt_dir);
				printf("\",\"fsname\":\"");
				escape_string(m->mnt_fsname);
				printf("\",\"type\":\"");
				escape_string(m->mnt_type);
				printf("\",\"opts\":\"");
				escape_string(m->mnt_opts);
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
						printf(RESET COLOR "%s" RESET " mounted at " COLOR "%s" RESET, m->mnt_fsname, m->mnt_dir);
					} else {
						printf("%s mounted at %s", m->mnt_fsname, m->mnt_dir);
					}
					printf(RESET ", ");
					if (vfs.f_blocks > 0) print_usage(color_flag, vfs.f_blocks, vfs.f_bfree, vfs.f_bavail, vfs.f_bsize);
					printf("\n");
				} else {
					if (color_flag) {
						printf(RESET COLOR "%s" RESET " mounted at " COLOR "%s" RESET "\n", m->mnt_fsname, m->mnt_dir);
						printf(RESET "type: " COLOR "%s" RESET ", opts: " COLOR "%s" RESET "\n", m->mnt_type, m->mnt_opts);
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

	int ret = 0;
	for (size_t i = 0; i < fs_count; ++i) {
		if (!fs[i]) continue;
		fprintf(stderr, "Filesystem %s not found\n", fs[i]);
		ret = 1;
	}

	return ret;
}
