# set SFPI release version information

sfpi_url=https://github.com/tenstorrent/sfpi/releases/download

# convert md5 file into these variables
# sed 's/^\([0-9a-f]*\) \*sfpi_\([-.0-9a-zA-Z]*\)_\([a-z0-9_A-Z]*\)\.\([a-z]*\)$/sfpi_version=\2'"\n"'sfpi_\3_\4_md5=\1/' *-build/sfpi_*.md5 | sort -u
sfpi_version=7.3.0-ext-29186
sfpi_x86_64_deb_md5=a72cd97dfedad8e81f47b3012e507116
sfpi_x86_64_rpm_md5=fa102f97a3557a749978f26c7a92b394
sfpi_x86_64_txz_md5=3d5ee7d5e966945313b500ba3aac6867
