.PHONY: all clean

CC          = gcc

GITHASH := $(shell git rev-parse --short HEAD)

#https://stackoverflow.com/questions/1079832/how-can-i-configure-my-makefile-for-debug-and-release-builds
CFLAGS      = \
    -I src/libdvbcsa/dvbcsa             \
	-finline-functions                  \
    -DGITHASH=\"$(GITHASH)\" \
    -MD

CFLAGS_SSE4_2		= \
    -msse4.2                            \
	-DENABLE_P_128_SSE2

CFLAGS_INT32		= \
	-DENABLE_P_32_INT

#################### sources ###################
libdvbcsa_src = \
	src/libdvbcsa/dvbcsa_algo.c     \
	src/libdvbcsa/dvbcsa_block.c    \
	src/libdvbcsa/dvbcsa_key.c      \
	src/libdvbcsa/dvbcsa_stream.c

SRCS = \
	src/main.c             \
	src/loop.c             \
	src/helper.c           \
	src/bs_algo.c          \
	src/bs_block.c         \
	src/bs_block_ab.c      \
	src/bs_sse2.c          \
	src/bs_stream.c        \
	src/bs_uint32.c        \
	src/ts.c               \
    $(libdvbcsa_src)

tsgen_src = tsgen.c    \
    $(libdvbcsa_src)

OBJS            = $(SRCS:%.c=%.o)
tsgen_obj       = $(tsgen_src:%.c=%.o)
EXE             = aycwabtu


#
# Debug build settings
#
DBGCFLAGS = -O0 -DDEBUG

DBGDIR_INT32 = debug/int32
DBGEXE_INT32 = $(DBGDIR_INT32)/$(EXE)
DBGOBJS_INT32 = $(addprefix $(DBGDIR_INT32)/, $(OBJS))

DBGDIR_SSE4_2 = debug/sse4_2
DBGEXE_SSE4_2 = $(DBGDIR_SSE4_2)/$(EXE)
DBGOBJS_SSE4_2 = $(addprefix $(DBGDIR_SSE4_2)/, $(OBJS))


#
# Release build settings
#
RELCFLAGS = -O3

RELDIR_INT32 = release/int32
RELEXE_INT32 = $(RELDIR_INT32)/$(EXE)
RELOBJS_INT32 = $(addprefix $(RELDIR_INT32)/, $(OBJS))

RELDIR_SSE4_2 = release/sse4_2
RELEXE_SSE4_2 = $(RELDIR_SSE4_2)/$(EXE)
RELOBJS_SSE4_2 = $(addprefix $(RELDIR_SSE4_2)/, $(OBJS))

.PHONY: all clean debug release remake dbg

# Default build
all: release debug

#
# Debug rules
#
debug: $(DBGEXE_INT32) $(DBGEXE_SSE4_2)
dbg: $(DBGEXE_SSE4_2)
.SECONDARY: debug release dbg

$(DBGDIR_INT32)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -MD $(CFLAGS) $(DBGCFLAGS) $(CFLAGS_INT32) $< -o $@

$(DBGEXE_INT32): $(DBGOBJS_INT32)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DBGCFLAGS) $(CFLAGS_INT32) $^ -o $(DBGEXE_INT32)
	@echo $@ created;echo

$(DBGDIR_SSE4_2)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -MD $(CFLAGS) $(DBGCFLAGS) $(CFLAGS_SSE4_2) $< -o $@

$(DBGEXE_SSE4_2): $(DBGOBJS_SSE4_2)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DBGCFLAGS) $(CFLAGS_SSE4_2) $^ -o $(DBGEXE_SSE4_2)
	@echo $@ created ; echo
	@gcc --version | head -n 1

#
# Release rules
#
release: $(RELEXE_INT32) $(RELEXE_SSE4_2)

$(RELDIR_INT32)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -MD $(CFLAGS) $(RELCFLAGS) $(CFLAGS_INT32) $< -o $@

$(RELEXE_INT32): $(RELOBJS_INT32)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(RELCFLAGS) $(CFLAGS_INT32) $^ -o $(RELEXE_INT32)
	@echo $@ created;echo

$(RELDIR_SSE4_2)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -MD $(CFLAGS) $(RELCFLAGS) $(CFLAGS_SSE4_2) $< -o $@

$(RELEXE_SSE4_2): $(RELOBJS_SSE4_2)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(RELCFLAGS) $(CFLAGS_SSE4_2) $^ -o $(RELEXE_SSE4_2)
	@echo $@ created;echo


# tsgen functionality 2b moved into main binary
tsgen: 
	echo TBD
	false

check:
	echo TBD
	false


clean:
	rm -rf release debug

include $(wildcard $(DBGDIR)/src/*.d $(RELDIR)/src/*.d)

