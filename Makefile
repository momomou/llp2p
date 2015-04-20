
CC = gcc
CPP = g++

#CPPFLAGS += -Wno-write-strings -g -W -I/usr/include -I/usr/local/include/stlport -I.
CPPFLAGS += -std=c++11 -g -W -I /usr/include -I /usr/local/include/stlport -I ./udt_lib -I. -pthread

#OBJS = configuration.o network.o peer.o basic_class.o pk_mgr.o peer_mgr.o logger.o librtsp/rtsp_server.o librtsp/rtsp_viewer.o librtmp/rtmp_server.o librtmp/rtmp_viewer.o librtsp/ss_rtsp.o stream.o stream_handler.o stream_handler_udp.o stream_server.o stream_udp.o
udt_lib_obj = udt_lib/api.o udt_lib/buffer.o udt_lib/cache.o udt_lib/ccc.o udt_lib/channel.o udt_lib/common.o udt_lib/core.o udt_lib/epoll.o udt_lib/list.o udt_lib/md5.o udt_lib/packet.o udt_lib/queue.o udt_lib/window.o
OBJS = basic_class.o bit_stream_server.o configuration.o io_accept.o io_accept_udp.o io_connect.o io_connect_udp.o io_connect_udp_ctrl.o io_nonblocking.o io_nonblocking_udp.o logger.o logger_client.o network.o network_udp.o peer.o peer_communication.o peer_mgr.o pk_mgr.o register_mgr.o stream.o stream_handler.o stream_handler_udp.o stream_udp.o
librtmp_obj = librtmp/parseurl.o librtmp/amf.o librtmp/rtmp.o librtmp/rtmp_supplement.o
libBitStream_obs = libBitStream/bit_stream_out.o
libBitStreamHTTP_obj = libBitStreamHTTP/bit_stream_httpout.o
libstunt_obj = stunt/tcp_punch.o
json_lib_obj = json_lib/json_reader.o json_lib/json_value.o json_lib/json_writer.o
stund_obj = stund/stun.o stund/udp.o

%.o: %.c
	$(CPP) $< $(CPPFLAGS) -c
	
all: ray

ray: $(libBitStreamHTTP_obj) $(libstunt_obj) $(udt_lib_obj) $(json_lib_obj) $(stund_obj) $(OBJS)  main.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^
#	strip $@

clean:
	rm -f *.o ray
	rm -f ./*/*.o

