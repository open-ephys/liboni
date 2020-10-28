PREFIX  := /usr/local/include/

.PHONY: install
install:
	@[ -d $(DESTDIR)$(PREFIX) ] || mkdir -p $(DESTDIR)$(PREFIX)oni
	cp oni.hpp onix.hpp $(DESTDIR)$(PREFIX)

.PHONY: uninstall
uninstall:
	$(RM) -r $(DESTDIR)$(PREFIX)oni.hpp
	$(RM) -r $(DESTDIR)$(PREFIX)onix.hpp
