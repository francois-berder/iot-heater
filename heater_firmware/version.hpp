#ifndef VERSION_H
#define VERSION_H

#define xstr(s) str(s)
#define str(s) #s

#ifndef GIT_HASH
#define GIT_HASH    "unknown"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME  "unknown"
#endif

#define FW_VERSION xstr(GIT_HASH) " - " xstr(BUILD_TIME)

#endif
