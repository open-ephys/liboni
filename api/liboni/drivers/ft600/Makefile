# "make help" prints help.
SHELL     :=  /bin/bash
NAME      :=  libonidriver_ft600
UNAME     :=  $(shell uname -s)
SRC       :=  onidriver_ft600.c circbuffer.c
OBJ       :=  $(SRC:.c=.o)
PREFIX    :=  /usr/local
LIBS      :=  -pthread
CFLAGS    :=  -pedantic -Wall -W -Werror -fPIC -O2 -fno-omit-frame-pointer -g

ifeq ($(UNAME), Linux)
DNAME     :=  $(NAME).so.1
DNAMELN   :=  $(NAME).so
FT_PATH   := ./ft3dxx/linux
LDFLAGS   :=  -L. -ldl -L$(FT_PATH) $(FT_PATH)/libftd3xx-static.a $(LIBS) -lstdc++
endif
ifeq ($(UNAME), Darwin)
DNAME     :=  $(NAME).dylib
DNAMELN   :=  $(NAME).1.dylib
FT_PATH   := ./ft3dxx/osx
LDFLAGS   :=  -L. -L$(FT_PATH) $(FT_PATH)/libftd3xx-static.a $(LIBS) -framework IOKit -framework CoreFoundation -lc++ -arch x86_64 -arch arm64
CFLAGS += -arch x86_64 -arch arm64
endif

CFLAGS += $(DEFS) -I$(FT_PATH)

# Turn wildcard list into comma separated list
SPACE :=
SPACE += # $SPACE is a SPACE
COMMA := ,
COMMA-SEPARATE = $(subst ${SPACE},${COMMA},$(strip $1))

.PHONY: all
all: $(DNAME) ## Make release version of onidriver_ft600.

.PHONY: debug
debug: CFLAGS += -DDEBUG -g3 ## Make driver with debug symbols.
debug: all

.PHONY: install 
install: $(SNAME) $(DNAME) ## Install driver. Defaults to make install PREFIX=/usr/local.
	@[ -d $(DESTDIR)$(PREFIX)/lib ] || mkdir -p $(DESTDIR)$(PREFIX)/lib
	@[ -d $(DESTDIR)$(PREFIX)/include ] || mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(DNAME) $(DESTDIR)$(PREFIX)/lib/$(DNAME)
	@[ -d $(DESTDIR)$(PREFIX)/lib/$(DNAMELN) ] || $(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)
	ln -s $(DESTDIR)$(PREFIX)/lib/$(DNAME) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)
ifeq ($(UNAME), Linux)
	ldconfig
	ldconfig -p | grep libonidriver
endif

.PHONY: uninstall
uninstall: ## Remove driver from installation directory.
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAME)
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)

ifeq ($(UNAME), Linux)
$(DNAME): LDFLAGS += -shared
endif
ifeq ($(UNAME), Darwin)
$(DNAME): LDFLAGS += -dynamiclib
endif

$(DNAME): $(OBJ) $(POSIX_OBJ)
ifeq ($(UNAME), Linux)
	$(CC) $^ -o $@ $(LDFLAGS)
endif
ifeq ($(UNAME), Darwin)
	$(CXX) $(LDFLAGS) $^ -o $@
endif

.PHONY: clean
clean: ## Remove local build objects
	$(RM) $(OBJ)
	$(RM) $(DNAME)

.PHONY: help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
