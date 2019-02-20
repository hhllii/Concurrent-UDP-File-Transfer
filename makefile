FTP:
	g++ -o ./bin/myclient -g ./src/TFTP_Client.cpp ./src/simpleSocket.cpp -lpthread
	g++ -o ./bin/myserver -g ./src/TFTP_Server.cpp ./src/simpleSocket.cpp -lpthread
clean:
	-rm -f ./bin/*
