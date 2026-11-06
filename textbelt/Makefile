
text_test: textbelt_utils.o
	cc -Wall text_test.c -o text_test textbelt_utils.o

textbelt_utils.o: textbelt_utils.c textbelt_utils.h
	cc -Wall -c textbelt_utils.c

clean:
	rm -f text_test *.o