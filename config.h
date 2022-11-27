#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "fileman"
#define BINDIR "/usr/bin"

//#define _(X) ((const char *) X)
//#define N_(X) ((const char *) X)
//#define __USE_MISC 1
//#define __USE_XOPEN_EXTENDED 1

//https://stackoverflow.com/questions/7090998/
#define UNUSED(x) (void)(x)

#define HAVE_LINUX 1

#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GRP_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MEMORY_H 1
#define HAVE_PATHS_H 1
#define HAVE_PWD_H 1
#define HAVE_SCHED_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_TIME_H 1

#define HAVE_GIO_UNIX 1
#define HAVE_LIBNOTIFY 1
//#define HAVE_GUDEV 1

#define HAVE_UNISTD_H 1

#endif // CONFIG_H


