
OUT=ctemplate
OBJS=generator.o objcopy.o main.o

$(OUT): $(OBJS)
	$(CC) -o $@ $^

main.o: main.c
	$(CC) -c $<

generator.o: generator.c generator.h common.h
	$(CC) -c $<

objcopy.o: objcopy.c objcopy.h common.h
	$(CC) -c $<
