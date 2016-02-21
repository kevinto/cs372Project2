#########################################################
# File: Makefile
# Description: A more robust makefile for CS162.
# You need to edit the variables under "USER SECTION"
#########################################################

# #'s are line-comments

# CXX is a standard variable name used for the compiler.
CXX = gcc

# CXXFLAGS is a standard variable name for compile flags.
# -std=c++0x specifies to use a certain language version.
CXXFLAGS = -std=c99

# -Wall turns on all warnings
CXXFLAGS += -Wall

# -pedantic-errors strictly enforces the standard
CXXFLAGS += -pedantic-errors

# -g turns on debug information
CXXFLAGS += -g

CXXFLAGS += -D_XOPEN_SOURCE=600

####################
### USER SECTION ###
####################

# SRCS is a standard name for the source files.
# Edit as needed.
SRC1 = server.c
SRCS = ${SRC1}

# HEADERS is a standard name for the header files.
# Edit these as needed.
# HEADER1 = 
# HEADERS = ${HEADER1}

# These will be your executable names.
# Edit as needed.
PROG1 = ftserver 
PROGS = ${PROG1}

#####################
### BUILD SECTION ###
#####################

# Typing 'make' in terminal calls the first build available.
# In this case, default.
default:
	${CXX} ${SRCS} -o ${PROG1}

# Typing 'make all' in terminal calls this build.
#all:
#	${CXX} ${CXXFLAGS} ${SRCS} ${HEADERS} -o ${PROG1}

# Typing 'make tar' in terminal calls this build.
# This creates a compressed file for submission.
#tar:
#	tar cvjf ${TAR} ${SRCS} ${HEADERS} ${DOC} makefile

# Typing 'make clean' calls this build.
# It's designed to clean up the folder.
# Be careful with this, edit as needed.
clean:
	rm -f ${PROGS} *.o *~ *.pyc

