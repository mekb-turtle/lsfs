% LSFS(1)
% mekb <mekb111@pm.me>
% lsfs

# NAME

**lsfs** - list mounts in a pretty way or in JSON

# SYNOPSIS

**lsfs** [options] [filesystems...]

# DESCRIPTION

Lists the currently mounted filesystems in a pretty way or in JSON.

Filesystems specified can either be the mount directory (e.g /), or the disk file (e.g /dev/sda1).

Omit the filesystems parameter to list all mounted filesystems.

# OPTIONS

-c, \--color, \--colour
: adds color to the output

-j, \--json
: outputs in json (incompatible with \--color)

-p, \--psuedofs
: outputs psuedo filesystems too

-q, \--quiet
: only show mount and block usage on 1 line

-t, \--tab
: split info by tab characters

# EXAMPLES

- `lsfs -cqt | column -ts$'\t'` - show in a pretty table

