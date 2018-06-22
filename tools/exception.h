#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

/*
 * Support for nested exception handling with backtrace and stacktrace.
 * 
 * Inspired by:
 *   https://en.cppreference.com/w/cpp/error/nested_exception
 *   https://stackoverflow.com/questions/37227300/why-doesnt-c-use-stdnested-exception-to-allow-throwing-from-destructor/37227893#37227893
 *   https://github.com/GPMueller/mwe-cpp-exception
 * 
 * Usage:
 *   try { THROW(...); }
 *   in catch() { RETHROW(...); }
 *   in the last catch() { BACKTRACE_WITH_RETHROW(...); }
 */

#include "tools/exception/stacktrace.h"

#define DEBUG_EXCEPTIONS

#ifndef DEBUG_EXCEPTIONS

	#define THROW(CLASS, MESSAGE)	\
		throw CLASS(MESSAGE)

	#define RETHROW(CLASS, MESSAGE)	\
		std::throw_with_nested( CLASS( MESSAGE ) )

	#define BACKTRACE_AND_RETHROW(CLASS, MESSAGE)	\
		std::rethrow_exception(std::current_exception());

#endif // DEBUG_EXCEPTIONS

#define TOOLS_STRINGIFY(x)		#x
#define TOOLS_TOSTRING(x)		TOOLS_STRINGIFY(x)
#define TOOLS_DEBUG_INFO		__FILE__ ":" TOOLS_TOSTRING(__LINE__)

#ifdef DEBUG_EXCEPTIONS
	// Throw a CLASS with some debug info
	// TODO: String contacenation can be done in preprocessor.

	#define THROW(CLASS, ARG, REST...)	\
		std::cerr << "------------------------------" << std::endl; \
		std::cerr << "THROWING " TOOLS_STRINGIFY(CLASS) << "( " << TOOLS_TOSTRING(ARG, ## REST) << " );" << " in " << std::string(TOOLS_DEBUG_INFO) << ": Function '" << __PRETTY_FUNCTION__ << '.' << std::endl; \
		print_stacktrace(); \
		throw CLASS( std::string(TOOLS_DEBUG_INFO) + ": Function '" + __PRETTY_FUNCTION__ + "': " + ARG, ## REST );

	#define RETHROW(CLASS, ARG, REST...)	\
		std::throw_with_nested( CLASS(std::string(TOOLS_DEBUG_INFO) + ": Exception thrown in '" + __PRETTY_FUNCTION__ + "': " + ARG, ## REST) );

	#define BACKTRACE_AND_RETHROW(CLASS, ARG, REST...)	\
		try { \
			RETHROW( CLASS, ARG, ## REST ); \
		} \
		catch(const std::exception& e) { \
			tools::exception::temporary::print_exception(e); \
			std::rethrow_exception(std::current_exception()); \
		} \

#endif // DEBUG_EXCEPTIONS


#include <cxxabi.h>
namespace tools {
	namespace exception {

		// Before it is moved to a final place, it is here
		struct temporary {
			// Prints the explanatory string of an exception. If the exception is nested,
			// recurses to print the explanatory of the exception it holds
			static void print_exception(const std::exception& e, int level =  0)
			{
				int status;
				if (level == 0) {
					std::cerr << "------------------------------" << std::endl;
					std::cerr << "EXCEPTION BACKTRACE:" << std::endl;
				}
				std::cerr << std::string(level << 1, ' ') << abi::__cxa_demangle(typeid(e).name(), 0, 0, &status) << " thrown in " << e.what() << '\n';
				try {
					std::rethrow_if_nested(e);
					std::cerr << "------------------------------" << std::endl;
				} catch(const std::exception& e) {
					print_exception(e, level+1);
				} catch(...) {}
			}
		};
	}
}





/* For the moment print_stacktrace() is used */

// #include <execinfo.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>

// #define BT_BUF_SIZE 100

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
