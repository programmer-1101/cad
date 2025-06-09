# Makefile

CC = gcc
SRC = main.c cJSON/cJSON.c
OUT = main.exe

all: run

build:
	$(CC) $(SRC) -o $(OUT)

run build:
	echo Running...
	$(OUT)

clean:
	del $(OUT)



