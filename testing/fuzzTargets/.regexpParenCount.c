static
int
parentheses(const char *re) {
	int c = 0;
	for (;;) {
		if (*re == '\0') {
			return c;
		}
		if (*re == '(') {
			c++;
			re++;
		} else if (*re == '\\') {
			re += 2;
		} else if (*re == '[') {
			for (;;) {
				if (*re == '\0') {
					__builtin_trap();
				}
				if (*re == '\\') {
					re += 2;
				} else if (*re == ']') {
					re++;
					break;
				}
			}
		}
	}
}
