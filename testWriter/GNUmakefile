#our makefile variables.   Good to place commonly changed variables
# at the top of your makefile. Then follow with your rules.

HOME = ./..

# We've made a common set of makefile definitions and rules in the home directory.
# By including that file, you have all the definitions you need.

include $(HOME)/GNUmakefile
CFLAGS += -O3
#CFLAGS += -g

testWriter: GNUmakefile $(OBJS) testWriter.cpp
	  $(CXX) $(CPPFLAGS) $(CFLAGS) -o testWriter.exe testWriter.cpp  $(OBJS)

