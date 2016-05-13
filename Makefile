all:
	gcc -o bi_udp_phone bi_udp_phone.c proto.c
	gcc -o bi_udp_PC bi_udp_PC.c proto.c

clean:
	rm -rf bi_udp_phone bi_udp_PC
