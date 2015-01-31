ifeq ($(OS),Windows_NT)
	include Makefile.Win
else
	ifeq ($(UNAME_S),Linux)
		include Makefile.Linux
    endif
    ifeq ($(UNAME_S),Darwin)
		include Makefile.OSX
    endif
endif
