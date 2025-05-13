ifeq ($(OS),Windows_NT)
	include Makefile.Win
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		include Makefile.Linux
	endif
ifeq ($(UNAME_S),Darwin)
		include Makefile.Macos
	endif
endif
