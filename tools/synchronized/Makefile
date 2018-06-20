MAKE_ALL= test_queue test_spinlock

CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -g -pthread

# work out names of object files from sources
OBJECTS = $(MAKE_ALL:=.o)

all:	$(MAKE_ALL)

test_queue: queue.h spinlock.h test_queue.cc
	$(CXX) $(CXXFLAGS) -o test_queue test_queue.cc -lpthread $(LDFLAGS)


test_spinlock: spinlock.h test_spinlock.cc 
	$(CXX) $(CXXFLAGS) -o test_spinlock test_spinlock.cc -lpthread $(LDFLAGS)

clean:
	rm ${OBJECTS} ${MAKE_ALL}

