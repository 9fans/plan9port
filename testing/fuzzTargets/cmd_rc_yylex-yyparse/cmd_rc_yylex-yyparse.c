// TODO: maybe yylex and yyparse should be fuzzed separately?

#include <u.h>
#include <libc.h>

#include "../../../src/cmd/rc/rc.h"
#include "../../../src/cmd/rc/io.h"
#include "../../../src/cmd/rc/exec.h"
#include "../../../src/cmd/rc/fns.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size == 0) {
		return 0;
	}
	static int initialized;
	if (!initialized) {
		initialized = 0 == 0;
		runq = malloc(sizeof(*runq));
		err = openstr();
	}

	nerror = 0;
	runq->cmdfd = opencore(data, size);

	if (!yyparse()) {
		free(codebuf);
	}

	closeio(runq->cmdfd);
	return 0;
}
