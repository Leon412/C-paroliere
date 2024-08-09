# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Iinclude -Wall -g -pedantic

# Paths
SRC_DIR = src
OBJ_DIR = build
INC_DIR = include

# Source files
SRV_SRC = $(SRC_DIR)/paroliere_srv.c $(SRC_DIR)/hashmap.c $(SRC_DIR)/list.c $(SRC_DIR)/messages.c $(SRC_DIR)/queue.c
CL_SRC = $(SRC_DIR)/paroliere_cl.c $(SRC_DIR)/hashmap.c $(SRC_DIR)/list.c $(SRC_DIR)/messages.c $(SRC_DIR)/queue.c

# Object files
SRV_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRV_SRC))
CL_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CL_SRC))


all: paroliere_srv paroliere_cl

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

paroliere_srv: $(SRV_OBJ)
	$(CC) $(SRV_OBJ) -o $(OBJ_DIR)/paroliere_srv

paroliere_cl: $(CL_OBJ)
	$(CC) $(CL_OBJ) -o $(OBJ_DIR)/paroliere_cl

clean:
	rm -rf $(OBJ_DIR)

.PHONY: all clean
# End of Makefile
