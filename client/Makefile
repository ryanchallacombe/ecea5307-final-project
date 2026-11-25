CFLAGS = -Wall -Werror -Iinclude/
UTILS_HEADER = include/utils.h
UTILS_SOURCE = include/utils.c
LFLAGS = -lm
CROSS_COMPILE_TB_SCRIPT_PATH = /aesd_client_include/tb_request.sh

## Invoke cross compilation with: >> make CROSS_COMPILE=aarch64-none-linux-gnu- all

## Line below not required because was have added the path to .bashrc file
# export PATH := $(PATH):/home/ryan/projects/aarch64_toolchain_install_dir/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin

ifdef CROSS_COMPILE
#@echo $(CFLAGS)
CFLAGS += -DTB_SCRIPT_PATH=\"$(CROSS_COMPILE_TB_SCRIPT_PATH)\"
$(info ***** CFLAGS is $(CFLAGS) *****)
else
$(info ***** CFLAGS is $(CFLAGS) *****)
endif

all: aesd_client
	@echo "**** Making all ..."

aesd_client: aesd_client.o utils.o
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -o aesd_client aesd_client.o utils.o ${LFLAGS}
else
	${CC} ${CFLAGS} -o aesd_client aesd_client.o utils.o ${LFLAGS}
endif

aesd_client.o: aesd_client.c ${UTILS_HEADER}
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -c aesd_client.c ${LFLAGS}
else
	${CC} ${CFLAGS} -c aesd_client.c ${LFLAGS}
endif

utils.o: ${UTILS_SOURCE} ${UTILS_HEADER}
ifdef CROSS_COMPILE
	$(CROSS_COMPILE)gcc ${CFLAGS} -c ${UTILS_SOURCE} ${LFLAGS}
else
	${CC} ${CFLAGS} -c ${UTILS_SOURCE} ${LFLAGS}
endif

clean:
	rm -f aesd_client *.o
	rm -f utils *.o