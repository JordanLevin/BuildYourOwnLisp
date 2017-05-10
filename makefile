all: prompt.c mpc.c mpc.h
	cc -std=c99 -Wall prompt.c mpc.c -ledit -g -lm -o prompt
