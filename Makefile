all:
	gcc -o udp_client udp_client.c proto.c
	gcc -o udp_server udp_server.c proto.c

clean:
	rm -rf udp_client udp_server
