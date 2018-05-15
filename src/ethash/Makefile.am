noinst_LIBRARIES = libethash.a

CFLAGS = -std=c99 -fPIC
if TARGET_WINDOWS
libethash_a_SOURCES = \
	io.c \
	internal.c \
	util.c \
	util_win32.c \
	mmap_win32.c \
	sha3.c \
	io_win32.c 
else
libethash_a_SOURCES = \
	io.c \
	internal.c \
	util.c \
	io_posix.c \
	sha3.c 
endif
