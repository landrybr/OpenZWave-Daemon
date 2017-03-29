CC=g++
CFLAGS=-g `./ozw_config --Cflags`
LDFLAGS=-g `./ozw_config --Libs` -pthread

make: ozwu_main.o ozwu.o
	$(CC) -o ozw_utility $(LDFLAGS) $(CFLAGS) ozwu_main.o ozwu.o

