#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <stdexcept>

//TODO: Add some debug info

// Throw with MESSAGE as it is
#define THROW(CLASS, MESSAGE)	\
	throw CLASS(MESSAGE)


#endif /* _EXCEPTION_H_ */
