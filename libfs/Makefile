# Target variables
SOURCES := disk.c fs.c
HEADERS := $(SOURCES: .c=.h)
OBJECTS := $(SOURCES:.c=.o)

CC := gcc
CFLAGS := -Wall -Werror
LIBFLAGS := ar rcs

# Target library
lib := libfs.a
all: $(lib)

# Dependency tracking
DEPS := $(patsubst %.o,%.d,$(OBJECTS))
-include $(deps)
DFLAGS = -MMD -MF $(@:.o=.d)

# additional output option
ifneq ($(V),1)
Q = @
endif

## debugging option
ifeq ($(D),1)
CFLAGS	+= -g
else
CFLAGS	+= -O2
endif

## generic rule for object files
%.o: %.c $(HEADERS)
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)

## generic rule for library
$(lib): $(OBJECTS)
	@echo "CC $@"
	$(Q)$(LIBFLAGS) $(lib) $^

clean:
	@echo "CLEAN"
	$(Q)rm -f $(OBJECTS) $(lib) $(DEPS)
