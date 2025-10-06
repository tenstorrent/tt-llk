#! /bin/bash

# Set SFPI release version information.
# This is the source of truth as to how we determine arch and distro names.
# Canonical location is in tenstorrent/tt-sfpi project's script directory.

# source $FILE -- generate sfpi variables for use by shell
# $FILE CMAKE -- emit cmake script to set variables
# $FILE *.md5 -- emit variable initializations from md5 file(s)

case "${1-}" in
    *.md5)
	# convert md5 files into release variables
	sed 's/^\([0-9a-f]*\) \*sfpi_\([-.0-9a-zA-Z]*\)_\([a-z0-9_A-Z]*\)\.\([a-z]*\)$/sfpi_version=\2'"\n"'sfpi_\3_\4_md5=\1/' "$@" \
	    | sort -u
	exit 0
	;;
esac

# define host system
sfpi_dist=unknown
if [[ -r /etc/os-release ]] ; then
    # some distros are sufficiently similar for common handling
    sfpi_dist=$(eval $(grep '^ID=' /etc/os-release) ; \
		case "$ID" in \
		    debian|ubuntu|redhat|centos|rhel|almalinux) ID=linux ;; \
		esac ; \
		echo "$ID")
fi
sfpi_arch=$(uname -m)

# download root location
sfpi_url=https://github.com/tenstorrent/sfpi/releases/download

# define release
sfpi_version=7.3.0-ext-29186
sfpi_x86_64_linux_deb_md5=4d9fb7ae2cb0ac4d374d8146009cb9fe
sfpi_x86_64_linux_rpm_md5=0c490e2fca4a26543f41e49a4741fb9a
sfpi_x86_64_linux_txz_md5=bb02d56a92012767bc33ceeb10246c07

case "${1-}" in
    CMAKE)
	# emit as cmake script
	for var in $(set -o posix ; set | grep '^sfpi_')
	do
	    # relies on no inserted quoting for meta-characters
	    name=${var%%=*}
	    echo "set(SFPI${name#sfpi} \"${var#*=}\")"
	done
	;;
esac
