##
# Linux Vars
##
CC = clang
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer -Wno-format-security -fsanitize=undefined -fsanitize=nullability -fsanitize=integer
CFLAGS = -Wall -Wextra 
DEBUG_FLAGS = -g $(ASAN_FLAGS) -D __COMPILE_DEBUG=1 -D __RAND_SEED=287091847 -D VERBOSE -D MAX_DEPTH=64 -D DEBUG
RELEASE_FLAGS = -O3 -Ofast -funroll-loops -flto -finline-functions -fexpensive-optimizations -fomit-frame-pointer -D __FAST_AS_POOP=1 -D STORE_STATS
PROFILE_FLAGS = -g -O2 -fno-lto -fno-omit-frame-pointer -pthread -D __PROFILE=1 -D __RAND_SEED=287091847
DEBUG_LDFLAGS += -lpthread $(ASAN_FLAGS) 
RELEASE_LDFLAGS += -lpthread
PROFILE_LDFLAGS += -lpthread
LINUX_TARGET_DEBUG = $(BINDIR)/chess_db
LINUX_TARGET_RELEASE = $(BINDIR)/chess
LINUX_TARGET_PROFILE = $(BINDIR)/chess_prof
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)


##
# Windows Vars
##
WIN_CC = x86_64-w64-mingw32-gcc
WIN_CFLAGS = -Wall -Wextra 
WIN_RELEASE_FLAGS = -pthread -O3 -Ofast -funroll-loops -flto -finline-functions -fexpensive-optimizations -fomit-frame-pointer -D __FAST_AS_POOP=1
WIN_DEBUG_FLAGS = -g -D __COMPILE_DEBUG=1 -D __RAND_SEED=287091847 -D VERBOSE -D MAX_DEPTH=64 -D DEBUG
WIN_LDFLAGS = -static-libgcc -static-libstdc++
WIN_TARGET_DEBUG = $(BINDIR)/chess_db.exe
WIN_TARGET_RELEASE = $(BINDIR)/chess.exe
WIN_OBJS = $(SRCS:src/%.c=$(OBJDIR)/win_%.o)


##
# Global Vars
##
SRCS := $(shell find src -type f -name '*.c')
BINDIR = bin
OBJDIR = obj


##
# Create Global Stuff
##
create_dirs:
	mkdir -p $(OBJDIR) $(addprefix $(OBJDIR)/, $(dir $(SRCS:src/%=%)))
	mkdir -p $(addprefix $(OBJDIR)/win_, $(dir $(SRCS:src/%=%)))

all: linux_debug windows_debug linux_release windows_release

debug: linux_debug windows_debug

profile: linux_profile

release: linux_release windows_release


$(BINDIR) $(OBJDIR):
	mkdir -p $@

##
# Linux Stuff
##
linux_debug: CFLAGS += $(DEBUG_FLAGS)
linux_debug: create_dirs $(LINUX_TARGET_DEBUG)

linux_release: CFLAGS += $(RELEASE_FLAGS)
linux_release: create_dirs $(LINUX_TARGET_RELEASE)

linux_profile: CFLAGS += $(PROFILE_FLAGS)
linux_profile: create_dirs $(LINUX_TARGET_PROFILE)

$(OBJDIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LINUX_TARGET_DEBUG): $(BINDIR) $(OBJS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(DEBUG_LDFLAGS) -o $(LINUX_TARGET_DEBUG) $(OBJS) $(DEBUG_LDFLAGS)

$(LINUX_TARGET_RELEASE): $(BINDIR) $(OBJS)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(RELEASE_LDFLAGS) -o $(LINUX_TARGET_RELEASE) $(OBJS) $(RELEASE_LDFLAGS)

$(LINUX_TARGET_PROFILE): $(BINDIR) $(OBJS)
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) $(PROFILE_LDFLAGS) -o $(LINUX_TARGET_PROFILE) $(OBJS) $(PROFILE_LDFLAGS)


##
# Windows Stuff
##
windows_debug: WIN_CFLAGS += $(WIN_DEBUG_FLAGS)
windows_debug: create_dirs $(WIN_TARGET_DEBUG)

windows_release: WIN_CFLAGS += $(WIN_RELEASE_FLAGS)
windows_release: create_dirs $(WIN_TARGET_RELEASE)

$(OBJDIR)/win_%.o: src/%.c
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

$(WIN_TARGET_DEBUG): $(BINDIR) $(WIN_OBJS)
	$(WIN_CC) $(WIN_CFLAGS) $(WIN_DEBUG_FLAGS) $(WIN_DEBUG_LDFLAGS) -o $(WIN_TARGET_DEBUG) $(WIN_OBJS) $(WIN_LDFLAGS)

$(WIN_TARGET_RELEASE): $(BINDIR) $(WIN_OBJS)
	$(WIN_CC) $(WIN_CFLAGS) $(WIN_RELEASE_FLAGS) $(WIN_RELEASE_LDFLAGS) -o $(WIN_TARGET_RELEASE) $(WIN_OBJS) $(WIN_LDFLAGS)


##
# Clean Up
##
clean:
	rm -f $(LINUX_TARGET) $(WIN_TARGET)
	rm -rf $(OBJDIR)
	rm -rf $(BINDIR)

clean_obj:
	rm -f $(LINUX_TARGET) $(WIN_TARGET)
	rm -rf $(OBJDIR)