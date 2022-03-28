# Indicate this is not a GNU project and that missing files
# should be automatically created in existing subdir "cfg"

AUTOMAKE_OPTIONS=foreign

ACLOCAL_AMFLAGS = -I cfg --install

# PROGRAMS indicate the name of executable(s) to be built.
# the prefix "bin" tells the install (if used) where to put it,
# bin_PROGRAMS = makeappimage makeappimageplugin
bin_PROGRAMS = makeappimage

# Define which sources a target is made of. No need to specify
# include files, they will be picked up by the dependency check.
makeappimage_SOURCES = \
	appdir.cpp \
	appdir_root_setup.cpp \
	copyright.cpp \
	copyright_dpkgquery.cpp \
	core.cpp \
	desktopfile.cpp \
	desktopfileentry.cpp \
	desktopfilereader.cpp \
	desktopfilewriter.cpp \
	elf_file.cpp \
	log.cpp \
	main.cpp \
	pipe_reader.cpp \
	process.cpp \
	subprocess.cpp \
	subprocess_result.cpp
