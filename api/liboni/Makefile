# "make help" prints help.
SHELL     :=  /bin/bash
NAME      :=  liboni
UNAME     :=  $(shell uname -s)
SNAME     :=  $(NAME).a
HDR       :=  oni.h onidefs.h onix.h onidriver.h # Public headers to be installed
SRC       :=  oni.c onix.c
POSIX_SRC :=  onidriverloader.c
OBJ       :=  $(SRC:.c=.o)
POSIX_OBJ :=  $(POSIX_SRC:.c=.o)
CFLAGS    :=  -Wall -W -Werror -fPIC -O3
ISOCFLAGS :=  -pedantic
PREFIX    :=  /usr/local

ifeq ($(UNAME), Linux)
DNAME     :=  $(NAME).so.1
DNAMELN   :=  $(NAME).so
LDFLAGS   :=  -L. -ldl -Wl,-rpath,'$$ORIGIN'
endif
ifeq ($(UNAME), Darwin)
DNAME     :=  $(NAME).dylib
DNAMELN   :=  $(NAME).1.dylib
LDFLAGS   :=  -L. -arch x86_64 -arch arm64
CFLAGS += -arch x86_64 -arch arm64
endif

# Turn wildcard list into comma separated list
SPACE :=
SPACE += # $SPACE is a SPACE
COMMA := ,
COMMA-SEPARATE = $(subst ${SPACE},${COMMA},$(strip $1))

.PHONY: all
all: $(SNAME) $(DNAME) ## Make release version of liboni.

.PHONY: debug
debug: CFLAGS += -DDEBUG -g3 ## Make liboni with debug symbols.
debug: all

.PHONY: install 
install: $(SNAME) $(DNAME) ## Install driver. Defaults to make install PREFIX=/usr
	@[ -d $(DESTDIR)$(PREFIX)/lib ] || mkdir -p $(DESTDIR)$(PREFIX)/lib
	@[ -d $(DESTDIR)$(PREFIX)/include ] || mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(DNAME) $(DESTDIR)$(PREFIX)/lib/$(DNAME)
	cp $(SNAME) $(DESTDIR)$(PREFIX)/lib/$(SNAME)
	cp $(HDR) $(DESTDIR)$(PREFIX)/include
	@[ -d $(DESTDIR)$(PREFIX)/lib/$(DNAMELN) ] || $(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)
	ln -s $(DESTDIR)$(PREFIX)/lib/$(DNAME) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)
ifeq ($(UNAME), Linux)
	ldconfig
	ldconfig -p | grep liboni
endif

.PHONY: uninstall
uninstall: ## Remove liboni from installation directory.
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(SNAME)
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAME)
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(DNAMELN)
	$(RM) $(DESTDIR)$(PREFIX)/include/{$(call COMMA-SEPARATE,${HDR})}

$(SNAME): $(OBJ) $(POSIX_OBJ)
	$(AR) $(ARFLAGS) $@ $^

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
	$(CC) $(LDFLAGS) $^ -o $@
endif

$(OBJ): CFLAGS += $(ISOCFLAGS)

.PHONY: clean
clean: ## Remove local build objects
	$(RM) $(OBJ) $(POSIX_OBJ)
	$(RM) $(SNAME) $(DNAME)

.PHONY: help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
