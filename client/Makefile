CFLAGS = -Wall -Werror -Iinclude/
UTILS_HEADER = include/utils.h
UTILS_SOURCE = include/utils.c

## Invoke cross compilation with: >> make CROSS_COMPILE=aarch64-none-linux-gnu- all

## Line below not required because was have added the path to .bashrc file
# export PATH := $(PATH):/home/ryan/projects/aarch64_toolchain_install_dir/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin

all: aesd_client

aesd_client: aesd_client.o utils.o
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -o aesd_client aesd_client.o utils.o 
else
	${CC} ${CFLAGS} -o aesd_client aesd_client.o utils.o 
endif

aesd_client.o: aesd_client.c ${UTILS_HEADER}
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -c aesd_client.c
else
	${CC} ${CFLAGS} -c aesd_client.c
endif

utils.o: ${UTILS_SOURCE} ${UTILS_HEADER}
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -c ${UTILS_SOURCE}
else
	${CC} ${CFLAGS} -c ${UTILS_SOURCE}
endif

clean:
	rm -f aesd_client *.o
	rm -f utils *.o