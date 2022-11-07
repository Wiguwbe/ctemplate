
#ifndef _CTEMPLATE_COMMON_H_
#define _CTEMPLATE_COMMON_H_

#include <stdio.h>
#include <errno.h>

#define PERROR(pre) fprintf(stderr, "%s::%s(%d) %s: %s\n" , __FILE__, __FUNCTION__, __LINE__, #pre, strerror(errno))

#endif
