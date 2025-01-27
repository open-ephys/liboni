# "make help" prints help.
SHELL     :=  /bin/bash
NAME      :=  oni-repl
SRC       :=  main.c
OBJ       :=  $(SRC:.c=.o)
CFLAGS    :=  -pedantic -Wall -W -Werror -fPIC -O3 $(DEFS)
# LDFLAGS   :=  -lm -lpthread -loni -ldl
PREFIX    :=  /usr/local
UNAME     :=  $(shell uname -s)

ifeq ($(UNAME), Linux)
LDFLAGS   :=  -lm -lpthread -loni -ldl
endif
ifeq ($(UNAME), Darwin)
LDFLAGS   := -L/usr/local/lib -lm -lpthread -loni
endif


.PHONY: all
all: $(NAME)

.PHONY: debug
debug: CFLAGS += -DDEBUG -g3 ## Build with debug symbols
debug: all

.PHONY: profile
profile: LDFLAGS += -lprofiler ## Link in the perftools profiler
profile: all

$(NAME): $(SRC) ## Make simple oni-repl acquisition software
	@echo Making $@
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $@

.PHONY: clean
clean: ## Clean build artifacts
	rm ./$(NAME)

.PHONY: install
install: $(NAME) ## Install the program. Defaults to make install PREFIX=/usr/local.
	@[ -d $(DESTDIR)$(PREFIX)/bin ] || mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)

.PHONY: uninstall
uninstall: ## Remove driver from installation directory.
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(NAME)

.PHONY: help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
