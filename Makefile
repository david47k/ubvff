CC=gcc
WIN32CC=i686-w64-mingw32-gcc
WIN64CC=x86_64-w64-mingw32-gcc
CFLAGS = -s -Wall -Wpedantic
WIN32_TARGETS = ubvff1.exe ubvff2.exe vecass.exe
WIN64_TARGETS = ubvff1.x64.exe ubvff2.x64.exe vecass.x64.exe
DEFAULT_TARGETS = ubvff1 ubvff2 vecass
ALL_TARGETS = $(WIN32_TARGETS) $(WIN64_TARGETS) $(DEFAULT_TARGETS)

default: $(DEFAULT_TARGETS)

all: $(ALL_TARGETS)

ubvff1  : ubvff1.c
	$(CC) $(CFLAGS) $^ -o ubvff1

ubvff1.exe  : ubvff1.c
	$(WIN32CC) $(CFLAGS) $^ -o ubvff1.exe

ubvff1.x64.exe : ubvff1.c
	$(WIN64CC) $(CFLAGS) $^ -o ubvff1.x64.exe

ubvff2  : ubvff2.c
	$(CC) $(CFLAGS) $^ -o ubvff2

ubvff2.exe : ubvff2.c
	$(WIN32CC) $(CFLAGS) $^ -o ubvff2.exe

ubvff2.x64.exe : ubvff2.c
	$(WIN64CC) $(CFLAGS) $^ -o ubvff2.x64.exe

vecass : vecass.c
	$(CC) $(CFLAGS) $^ -o vecass

vecass.exe  : vecass.c
	$(WIN32CC) $(CFLAGS) $^ -o vecass.exe

vecass.x64.exe : vecass.c
	$(WIN64CC) $(CFLAGS) $^ -o vecass.x64.exe

.PHONY : clean
clean:
	rm $(ALL_TARGETS)

