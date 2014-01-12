CC=gcc
CFLAGS=-Wall -O3 -ISDL/include -IS:/Programs/MinGW/msys/1.0/local/include
LIBS=-LSDL/lib -LS:/Programs/MinGW/msys/1.0/local/lib -mwindows -lmingw32 -lSDLmain -lSDL -lpng -lz -ljpeg
ZIPLIBS=-LS:/Programs/MinGW/msys/1.0/local/lib -lz
OBJECTS=main.o junzip.o image.o font.o jview.res

all: jview.exe

run: jview.exe
	./$^ test.zip
	
clean:
	$(RM) *.o *.exe

jview.exe: $(OBJECTS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.exe: %.o
	$(CC) $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.res: %.rc
	windres $< -O coff -o $@

# Small helpers to make point.hpp inline changes also recompile these files
image.o: image.c image.h
font.o: font.c font.h
jview.res: jview.ico
