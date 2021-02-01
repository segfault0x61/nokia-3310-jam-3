###############################################################################
# Variables
###############################################################################
CC = gcc
CC_FLAGS = -D_GNU_SOURCE -std=c99 -Wfatal-errors
SRC = ./src/*.c

ifeq ($(OS),Windows_NT) 
	LINKER_FLAGS = -lmingw32 -mwindows
else
	LINKER_FLAGS = -lm 
endif

LINKER_FLAGS += -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer

BIN_NAME = game

###############################################################################
# Makefile rules
###############################################################################
build:
	${CC} ${CC_FLAGS} ${SRC} ${LINKER_FLAGS} -o ${BIN_NAME}

run:
	./${BIN_NAME}

clean:
	rm ${BIN_NAME}