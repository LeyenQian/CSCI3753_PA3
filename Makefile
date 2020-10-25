# Makefile v1 for CSCI3753-F20 PA3

CC = gcc
CFLAGS = -Wextra -Wall -g
INCLUDES = 
LFLAGS = 
LIBS = -lpthread

MAIN = multi-lookup

# Add any additional .c files to SRCS and .h files to HDRS
SRCS = multi-lookup.c util.c
HDRS = multi-lookup.h util.h

OBJS = $(SRCS:.c=.o) 

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean: 
	$(RM) *.o *~ $(MAIN)

SUBMITFILES = $(SRCS) $(HDRS) Makefile README performance.txt
submit: 
	@read -r -p "Enter your identikey username: " username; \
	echo; echo Bundling the following files for submission; \
	tar --transform "s|^|PA3-$$username/|" -cvf PA3-$$username.txt $(SUBMITFILES); \
	echo; echo Please upload the file PA3-$$username.txt to Canvas to complete your submission; echo
