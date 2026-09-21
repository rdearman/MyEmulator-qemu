/* Hosted Linux additions for the MyEmulator2 target. */
#ifndef GCC_MYEMULATOR2_LINUX_H
#define GCC_MYEMULATOR2_LINUX_H

#undef STARTFILE_SPEC
#define STARTFILE_SPEC GNU_USER_TARGET_STARTFILE_SPEC
#undef ENDFILE_SPEC
#define ENDFILE_SPEC GNU_USER_TARGET_ENDFILE_SPEC
#undef LIB_SPEC
#define LIB_SPEC GNU_USER_TARGET_LIB_SPEC
#undef LINK_SPEC
#define LINK_SPEC "%{!T*:-T%R/lib/myemulator2-user.ld} %{static:-Bstatic}"

#endif
