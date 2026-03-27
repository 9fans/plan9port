%:
	@echo read the README file

clean:
	find src -name '*.o' -delete
	find src -name 'o.*' -type f -delete
	find src -name 'y.tab.[ch]' -delete
	find src -name 'a.out' -delete
	rm -f src/cmd/devdraw/latin1.h
	rm -f src/cmd/delatex.c

distclean: clean
	rm -f lib/*.a
	rm -f config install.log install.sum
	git ls-files --others --exclude-standard bin/ | xargs -r rm -f
