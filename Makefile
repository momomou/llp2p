
CC = gcc
CPP = g++

CPPFLAGS += -Wno-write-strings -g -W -I/usr/include -I/usr/local/include/stlport -I.

OBJS = configuration.o network.o peer.o basic_class.o pk_mgr.o peer_mgr.o logger.o rtsp_server.o rtsp_viewer.o rtmp_server.o rtmp_viewer.o ss_rtsp.o stream.o stream_handler.o stream_handler_udp.o stream_server.o stream_udp.o
librtmp_obj = parseurl.o amf.o rtmp.o rtmp_supplement.o

all: ray

ray: $(librtmp_obj) ${OBJS} main.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^
#	strip $@

clean:
	rm -f *.o ray

