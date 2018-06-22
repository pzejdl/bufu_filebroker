#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include "tools/exception/stacktrace.h"

#define DEBUG_EXCEPTIONS

#ifndef DEBUG_EXCEPTIONS
// Throw with MESSAGE as it is
	#define THROW(CLASS, MESSAGE)	\
		throw CLASS(MESSAGE)
#endif // DEBUG_EXCEPTIONS


#define TOOLS_STRINGIFY(x)		#x
#define TOOLS_TOSTRING(x)		TOOLS_STRINGIFY(x)
#define TOOLS_DEBUG_INFO		__FILE__ ":" TOOLS_TOSTRING(__LINE__)

#ifdef DEBUG_EXCEPTIONS
// Throw a CLASS compatible with tools::exception with an additional information
	#define THROW(CLASS, ARG, REST...)	\
		print_stacktrace(); \
		throw CLASS(std::string(TOOLS_DEBUG_INFO) + ": Exception thrown in '" + __PRETTY_FUNCTION__ + "': " + ARG, ## REST);
#endif // DEBUG_EXCEPTIONS





#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BT_BUF_SIZE 100

// void stacktrace()
// {
// 	int j, nptrs;
// 	void *buffer[BT_BUF_SIZE];
// 	char **strings;

// 	nptrs = backtrace(buffer, BT_BUF_SIZE);
// 	printf("backtrace() returned %d addresses (skipping the first one):\n", nptrs);

// 	/* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
// 		would produce similar output to the following: */

// 	strings = backtrace_symbols(buffer, nptrs);
// 	if (strings == NULL) {
// 		perror("backtrace_symbols");
// 		exit(EXIT_FAILURE);
// 	}

// 	for (j = 1; j < nptrs; j++)
// 		printf("  %s\n", strings[j]);

// 	free(strings);
// }


#endif /* _EXCEPTION_H_ */
