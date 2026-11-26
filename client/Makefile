CFLAGS = -Wall -Werror -Iinclude/
UTILS_HEADER = include/utils.h
UTILS_SOURCE = include/utils.c
LFLAGS = -lm
TB_PATH = tb_request.sh
CFLAGS += -DTB_SCRIPT_PATH=\"$(TB_PATH)\"
$(info ***** CFLAGS is $(CFLAGS) *****)

all: aesd_client
	@echo "**** Making all ..."

aesd_client: aesd_client.o utils.o
	${CC} ${CFLAGS} -o aesd_client aesd_client.o utils.o ${LFLAGS}

aesd_client.o: aesd_client.c ${UTILS_HEADER}
	${CC} ${CFLAGS} -c aesd_client.c ${LFLAGS}

utils.o: ${UTILS_SOURCE} ${UTILS_HEADER}
	${CC} ${CFLAGS} -c ${UTILS_SOURCE} ${LFLAGS}

clean:
	rm -f aesd_client *.o
	rm -f utils *.o