lib = -lmicrohttpd
lib2 = curl
libJSON = -ljson-c

RSU_server: RSU_server.c
	gcc RSU_server.c -g -o RSU_server $(lib) $(libJSON)

RSU_client: RSU_client.c
	gcc RSU_client.c -g -l $(lib2) -o RSU_client

clean:
	rm -rf RSU_server RSU_client