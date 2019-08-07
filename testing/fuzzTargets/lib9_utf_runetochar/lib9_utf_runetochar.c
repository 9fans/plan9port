#include <u.h>
#include <libc.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size != sizeof(Rune)) {
		return 0;
	}
	char s[UTFmax];
	Rune r;
	memcpy(&r, data, sizeof(r));
	int bytes = runetochar(s, &r);
	if (bytes < 1 || UTFmax < bytes) {
		__builtin_trap();
	}
	return 0;
}
