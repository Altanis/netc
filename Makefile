# Compiler settings
CC := clang
CFLAGS := -Iinclude -Ofast
LDLIBS :=

# Source file directories
SRC_DIR := src
INCLUDE_DIR := include

# Source files
CLIENT_SRCS := $(wildcard $(SRC_DIR)/client.c)
SERVER_SRCS := $(wildcard $(SRC_DIR)/server.c)
VECTOR_SRCS := $(wildcard $(SRC_DIR)/vector.c)
ERROR_SRCS := $(wildcard $(SRC_DIR)/error.c)
MAIN_SRCS := $(wildcard main.c)

# Output binary
OUTPUT := netc

all: $(OUTPUT)

$(OUTPUT): $(CLIENT_SRCS) $(SERVER_SRCS) $(VECTOR_SRCS) $(ERROR_SRCS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(OUTPUT)
