# example makefile
#
# Notes:
# - the indents must be tabs
# - if you need to specify a non-standard compiler, then set CXX
#   (it defaults to g++)
# - if you need to pass flags to the linker, set LDFLAGS rather
#   than alter the ${TARGET}: ${OBJECTS} rule
# - to see make's built-in rules use make -p

# target executable name
TARGET = main

# source files
SOURCES = main.cpp src/connection.cpp src/connection_manager.cpp src/mime_types.cpp src/reply.cpp src/request_handler.cpp src/request_parser.cpp src/server.cpp

# work out names of object files from sources
OBJECTS = $(SOURCES:.cpp=.o)

# compiler flags (do not include -c here as it's dealt with by the
# appropriate rules; CXXFLAGS gets passed as part of command
# invocation for both compilation (where -c is needed) and linking
# (where it's not.)
#CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -g 
CXXFLAGS = -std=c++11 -Wall -Wextra -rdynamic -O0 -g -pthread

LDFLAGS = -L/opt/local/lib -lboost_system-mt

CPPFLAGS = -I. -Isrc -I/opt/local/include


# default target (to build all)
all: ${TARGET}

# clean target
clean:
	rm ${OBJECTS} ${TARGET}

# rule to link object files to create target executable
# $@ is the target, here $(TARGET), and $^ is all the
# dependencies separated by spaces (duplicates removed),
# here ${OBJECTS}
${TARGET}: ${OBJECTS}
	${LINK.cpp} -o $@ $^

# no rule is needed here for compilation as make already
# knows how to do it

# header dependencies (in more complicated cases, you can
# look into generating these automatically.)

#product.o : product.h test1.h test2.h

#test1.o : product.h test1.h test2.h

#test2.o : product.h test2.h

src/server.o:           src/server.hpp
src/request_parser.o:   src/request_parser.hpp src/request.hpp
src/request_handler.o:  src/request_handler.hpp src/request_handler.cpp
src/reply.o:            src/reply.hpp
src/mime_types.o:       src/mime_types.hpp
src/connection_manager.o:       src/connection_manager.hpp
src/connection.o:       src/connection.hpp src/connection_manager.hpp src/request_handler.hpp
