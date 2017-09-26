TOP_DIR := $(shell pwd)
APP = $(TOP_DIR)/bin/trans_mips_demo

CC = mipsel-openwrt-linux-gcc
LIBS = -lpthread -ljson -lm -lMagickWand-7.Q16HDRI -lzbar -liconv -ldl -lMagickCore-7.Q16HDRI -ljpeg

DEP_INC = include
DEP_LIB = sta_lib
EXT_SET = -DMAGICKCORE_QUANTUM_DEPTH=16 -DMAGICKCORE_HDRI_ENABLE=1
CFLAGS = -I$(DEP_INC) -L$(DEP_LIB)

OBJS = main.o thread.o video_capture.o mjpegencode.o sensor_send.o
all: $(OBJS)
	$(CC) -o $(APP) $(OBJS) $(LIBS) $(CFLAGS)

main.o:main.c
thread.o:thread.c
video_capture.o:video_capture.c
mjpegencode.o:mjpegencode.c
	$(CC) -c $< -I include $(EXT_SET)
sensor_send.o:sensor_send.c
	$(CC) -c $< -I include

.PHONY:clean

clean:
	rm -f *.o a.out $(APP) *~
