# target executable name
TARGET = http_client

# source files
SOURCES = http_client.cc

# work out names of object files from sources
OBJECTS = $(SOURCES:.cc=.o)

CXXFLAGS = -std=c++11 -Wall -Wextra -pthread

LDFLAGS = -lboost_system-mt

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
	${LINK.cc} -o $@ $^
