include Makefile.common
CFLAGS += -DHAVE_POLL

NOGETRANDOM := $(shell echo "\#include <sys/random.h>\nint main() { unsigned int r; getrandom(&r, sizeof(r), 0);}" | $(CC) -o /dev/null -Werror -xc - >/dev/null 2>/dev/null && echo 0 || echo 1)
ifeq "$(NOGETRANDOM)" "0"
    CFLAGS += -D__GETRANDOM_DEFINED__
endif
