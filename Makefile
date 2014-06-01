
CC = gcc
CPP = g++

#CPPFLAGS += -Wno-write-strings -g -W -I/usr/include -I/usr/local/include/stlport -I.
CPPFLAGS += -g -W -I/usr/include -I/usr/local/include/stlport -I.

#OBJS = configuration.o network.o peer.o basic_class.o pk_mgr.o peer_mgr.o logger.o librtsp/rtsp_server.o librtsp/rtsp_viewer.o librtmp/rtmp_server.o librtmp/rtmp_viewer.o librtsp/ss_rtsp.o stream.o stream_handler.o stream_handler_udp.o stream_server.o stream_udp.o
OBJS = basic_class.o bit_stream_server.o configuration.o io_accept.o io_connect.o io_nonblocking.o logger.o logger_client.o network.o peer.o peer_communication.o peer_mgr.o pk_mgr.o register_mgr.o stream.o stunt_mgr.o
librtmp_obj = librtmp/parseurl.o librtmp/amf.o librtmp/rtmp.o librtmp/rtmp_supplement.o
libBitStreamHTTP_obj = libBitStreamHTTP/bit_stream_httpout.o
libstunt_obj = stunt/tcp_punch.o

all: ray

ray: $(libBitStreamHTTP_obj) $(libstunt_obj) ${OBJS} main.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^
#	strip $@

clean:
	rm -f *.o ray

