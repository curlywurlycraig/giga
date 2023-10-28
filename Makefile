OBJECTS = edit.o main.o

giga: $(OBJECTS)
	cc -o giga $(OBJECTS)

edit.o: edit.c
	cc -c edit.c

main.o: main.c
	cc -c main.c

gendump: dump.c
	cc dump.c -o dump
	./dump > dump_out.txt

clean:
	rm *.o giga dump dump_out.txt

.PHONY: build clean
