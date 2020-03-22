#include <iostream>         //Main
#include <sys/socket.h>     //Sockets
#include <string.h>         //Strings
#include <arpa/inet.h>      //Converter
#include <thread>           //Threads
#include <unistd.h>
#include <ctime>          //Time
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <mutex>
#include <map>
#include <netinet/tcp.h>
#define MAX_EVENTS 10000


#include "BanList.h"


int setnonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


int socket_connect( const char* ip, int port)
{
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(ip);

	
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		return -1;	
	return sockfd;
	
}

struct Connection
{
	uint32_t ip;
	int src;
	int dst;
	int offset = 0;
	int len = 0;
	int buf_size = 8190;
	unsigned char buffer[8192];
	
	int packet_offset = 0;
	int packet_len = 0;
	bool is_server = false;
	bool is_handshaked = false;
};



class Redirect
{
	public:
		std::string ip;
		int port;
};





int readVarInt(Connection* dst, int& bytes_read, int correction) {
    int numRead = 0;
    int result = 0;
	//++dst->packet_offset;
    unsigned char read;
    do {
        read = dst->buffer[dst->packet_offset + correction];
        int value = (read & 0b01111111);
        result |= (value << (7 * numRead));

	++numRead;
	++dst->packet_offset;
        
        if (numRead > 5) {
            return -1;
        }
	
    } while ((read & 0b10000000) != 0);
	
	
	bytes_read = numRead;
    return result;
}










int inspection(Connection* src, Connection* dst, BanList* ban_list, std::map <int,Connection*> *connectionList)
{

	//THIS IS PACKET DECODER CODE
	//DO NOT TOUCH ANYTHING!
	
	if(src->is_handshaked || src->is_server)	//Do not serve other packets (except handshake)
		return 0;
	
	
	dst->packet_offset = dst->packet_len;
	dst->packet_len = 0;
	
	while(dst->packet_offset < dst->len )
	{
		int bytes_read = 0;
		int size = readVarInt(dst, bytes_read, 0);
		int id = readVarInt(dst, bytes_read, 1);
		//int protocol_version = readVarInt(dst, bytes_read, 0);
		if(!src->is_handshaked && !src->is_server)
		{
			++dst->packet_offset;
			int nickname_len = readVarInt(dst, bytes_read,0);
			dst->packet_offset += nickname_len;
			
			
			dst->packet_offset += 2;
			uint32_t next_state = readVarInt(dst, bytes_read, 0);
			//std::cout << "Caught handshake id= " << id <<" state= " << next_state << std::endl;
			
			if(next_state > 2 || next_state < 1 || id > 340 || id < 250)	//wrong handshake packet. Starting BAN MACHINE!
			{
				ban_list->add(src->ip, 15);//ban ip addr for 15 minutes
				close(src->src);
				close(src->dst);
				
				
				
				connectionList->erase(src->src);
				connectionList->erase(dst->src);
				delete src;
				delete dst;
				
				return 0;
				//Banned successfully nahuy
			}
			
			src->is_handshaked = true;
			return 0;
		}
		
		dst->packet_offset += size - bytes_read;
	}

	
	if(dst->packet_offset > dst->len)
		dst->packet_len = dst->packet_offset - dst->len;
	
	dst->packet_offset = 0;
	
	
	return 0;
}





int main()
{

	printf("Welcome to Plant proxy (Version 1.0)\n");
	
	BanList ban_list;


	std::map <int,Connection*> connectionList;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	int server_sock =     socket(AF_INET, SOCK_STREAM, 0);
	int port = 25577;
	
	int listen_res, epollfd, connection;
	
	
	struct sockaddr_in serv_addr;				
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	
	
	
	struct epoll_event ev, events[MAX_EVENTS];
	
	
	
	if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		std::cout << "[ERROR] Binding error. Port " << port << " is busy" << std::endl;
	else
		std::cout << "[OK] Binding at port " << port << std::endl;
	
	
	setnonblocking(sock);
    	epollfd = epoll_create(1);
    	ev.events = EPOLLIN;// | EPOLLET;
    	ev.data.fd = sock;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1)
        	std::cout << "[ERROR] epoll_ctl(): server's socket" << std::endl;
	
	

	while(true)
	{
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        	if (nfds == -1) 
		{
			std::cout << "[ERROR] nfds error: Err_code=" << errno << std::endl;
			return 0;
		}
		


		for (register size_t k = 0; k < nfds; ++k)
		{
			if(events[k].data.fd == sock)							//если есть запрос на подключение, принимаем.
			{            
				listen_res = listen(events[k].data.fd, 0);
                
			 	if(listen_res = 1)
                		{
					connection = accept(sock, (struct sockaddr*)&from, &len);
					
					if(ban_list.check(from.sin_addr.s_addr) > 0 )
					{
						//printf("[debug] here1, banned\n");
						close(connection);
						
						continue;
					}
					
					if(time(0) > ban_list.check(from.sin_addr.s_addr))
					{
						ban_list.remove(from.sin_addr.s_addr);
					}
					
					if(connection == -1)
						continue;
					
					if (setnonblocking(connection) == -1)
						printf("\033[01;31m[ERROR]\033[0m setnonblocking()\n");

					
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
					//ev.events = EPOLLRDHUP | EPOLLERR | EPOLLET | EPOLLOUT;
					ev.data.fd = connection;

					int server = socket_connect("51.68.139.22", 25565);

					setnonblocking(server);
					int yes = 1;
					int result = setsockopt(server,
								IPPROTO_TCP,
								TCP_NODELAY,
								(char *) &yes, 
								sizeof(int));

					setsockopt(connection,
								IPPROTO_TCP,
								TCP_NODELAY,
								(char *) &yes, 
								sizeof(int));
					
					
					Connection* client_connection = new Connection();
					client_connection->ip = from.sin_addr.s_addr;
					client_connection->src = connection;
					client_connection->dst = server;
					client_connection->offset = 0;
					client_connection->is_server = false;

					Connection* server_connection = new Connection();
					server_connection->src = server;
					server_connection->dst = client_connection->src;
					server_connection->offset = 0;	
					server_connection->is_server = true;				
					connectionList.insert(std::pair<int, Connection*>(server, server_connection));
					
					connectionList.insert(std::pair<int, Connection*>(connection, client_connection));
					
					
					if(epoll_ctl(epollfd, EPOLL_CTL_ADD, connection, &ev) == -1)
						printf("\033[01;31m[ERROR]\033[0m epoll_ctl(connection) returned an error\n");
					//else
					//	printf("\033[01;33m[INFO]\033[0m New Client's IP: %s   [%d]\n", inet_ntoa(from.sin_addr), client_connection->src);
		                    	
					ev.data.fd = server;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, server, &ev);
				}
				
			}
			else
			{

				
				if (events[k].events & EPOLLIN)
				{
					
					
					
					if(connectionList.find(events[k].data.fd) != connectionList.end())
					{
						Connection* src = connectionList.find(events[k].data.fd)->second;
						
						if(connectionList.find(src->dst) == connectionList.end())
							continue;
						Connection* dst = connectionList.find(src->dst)->second;

						int ret = 0;
						
						do 
						{
							errno = 0;
							if(dst->len != 0)
								break;
							
							ret = recv(events[k].data.fd, dst->buffer,dst->buf_size,0);
							
							if(ret <= 0)
								break;
							
							
							dst->len = ret;
							dst->offset = 0;

							inspection(src, dst, &ban_list, &connectionList);
							
							
							epoll_event Eevent;
							Eevent.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLET;
							Eevent.data.fd = dst->src;
							epoll_ctl(epollfd, EPOLL_CTL_MOD, dst->src, &Eevent);
							
						}
						while(errno != EAGAIN && ret > 0 && errno != EWOULDBLOCK);
						
					}

		       		}


				if (events[k].events & EPOLLOUT)
				{
					
					if(connectionList.find(events[k].data.fd) != connectionList.end())
					{
						Connection* dst = connectionList.find(events[k].data.fd)->second;
						
						//if(dst->src == -1)
						//	continue;
						
						if(dst->len != 0)
						{
							int ret = send(dst->src, dst->buffer, dst->len,MSG_NOSIGNAL);
						
							if(ret <= 0)
								continue;
						
							dst->offset += ret;
							dst->len -= ret;
						}
						
						if(dst->len == 0)
						{
							epoll_event Eevent;
							Eevent.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
							Eevent.data.fd = events[k].data.fd;
							epoll_ctl(epollfd, EPOLL_CTL_MOD, events[k].data.fd, &Eevent);
						}
					}
				}

				if (events[k].events & (EPOLLRDHUP | EPOLLHUP))
				{
					//printf("\033[01;33m[INFO]\033[0m Disconnect event\n");
					
					if(connectionList.find(events[k].data.fd)!= connectionList.end())
					{
						if(connectionList.find(connectionList.find(events[k].data.fd)->second->dst)!= connectionList.end())
						{
							delete connectionList.find(connectionList.find(events[k].data.fd)->second->dst)->second;
							connectionList.erase(connectionList.find(events[k].data.fd)->second->dst);
							close(connectionList.find(events[k].data.fd)->second->dst);
							
						}
						delete connectionList.find(events[k].data.fd)->second;
						connectionList.erase(events[k].data.fd);
						close(events[k].data.fd);
						
					}
					
			
				}

       			}
				
				
				
				
				
			
		}
	}

}
		
		
		
		
		


