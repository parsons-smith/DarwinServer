/*
    CMSServer.cpp
    Based on epoll
    2015.05.21
    Author:Luoo
*/
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>   
#include <sys/epoll.h> 
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "tinyxml/tinyxml.h"
#include <iostream>
#include <exception>
using namespace std;


#define MAX_EPOLL_SIZE 5000
#define MAX_DATA_SIZE 4098
#define SERVER_PORT 8000
#define MAX_CONN_NUM 5000   
int cur_conn;   //Current connection
int bytenum;  // recv buffer size
char buf[MAX_DATA_SIZE];    //recv buffer
int storage_fd = -1;
int video_fd = -1;
int proxy_fd = -1;
int onvif_fd = -1;
TiXmlDocument* handle;


int message_handle(int connfd); //hand incoming message
int xml_decode(char * xml, int sock_fd); //decode xml message and resend it
int setnonblocking(int sockfd); //set a socket nonblocking
int compare(int a, int b);  //comapre two integer if they equals
void check_vpso(int conn_fd);  //check if onvif,proxy,video,storage server


int main(int argc, char **argv){
    
    int listenfd, connfd, ep_fd, nfds, n;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event ev;
    struct epoll_event events[MAX_EPOLL_SIZE];
    char buf[MAX_DATA_SIZE];

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
    servaddr.sin_port = htons (SERVER_PORT);

    listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd == -1){
        perror("ERROR:Can't create socket file\n");
        return -1;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (setnonblocking(listenfd) < 0) {
        perror("ERROR:Setnonblock error\n");
    }
    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr)) == -1){
        perror("ERROR:Bind error\n");
        return -1;
    } 
    if (listen(listenfd, MAX_CONN_NUM) == -1){
        perror("ERROR:Listen error\n");
        return -1;
    }
    ep_fd = epoll_create(MAX_EPOLL_SIZE);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, listenfd, &ev) < 0) {
        fprintf(stderr, "ERROR:Epoll set insertion error: fd=%d\n", listenfd);
        return -1;
    }
    cur_conn = 1;
    printf("INFO:CMSServer startup...\nINFO:Listening port %d\nINFO:Max connection is %d\n", SERVER_PORT, MAX_EPOLL_SIZE);

    for (;;) {
        nfds = epoll_wait(ep_fd, events, cur_conn, -1);
        if (nfds == -1){
            perror("ERROR:Epoll_wait\n");
            continue;
        }
        for (n = 0; n < nfds; ++n){
            //handle connection
            if (events[n].data.fd == listenfd){
                connfd = accept(listenfd, (struct sockaddr *)&cliaddr,&socklen);
                if (connfd < 0){
                    perror("ERROR:Accept error");
                    continue;
                }
                sprintf(buf, "Accept from %s:%d", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                printf("INFO:%s || Current connection is %d\n", buf, cur_conn);
                
                //exceed the max connection limit
                if (cur_conn >= MAX_EPOLL_SIZE){
                    fprintf(stderr, "WARNING:Too many connection, more than %d\n", MAX_EPOLL_SIZE);
                    close(connfd);
                    continue;
                } 
                
                if (setnonblocking(connfd) < 0){
                    perror("ERROR:Setnonblocking error\n");
                }
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;
                if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, connfd, &ev) < 0){
                    fprintf(stderr, "ERROR:Add socket '%d' to epoll failed: %s\n", connfd, strerror(errno));
                    return -1;
                }
                cur_conn++;
                continue;
            } 
            //handle other  message
            if (message_handle(events[n].data.fd) < 0){
                epoll_ctl(ep_fd, EPOLL_CTL_DEL, events[n].data.fd,&ev);
                cur_conn--;
            }
        }
    }
    close(listenfd);
    return 0;
}




int setnonblocking(int sockfd){
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1){
        return -1;
    }
    return 0;
}




int message_handle(int connfd) {
    bytenum = recv(connfd, buf, MAX_DATA_SIZE, 0);
    if (bytenum == 0){
        cout << "INFO:Client close the connection || Current connection is " << cur_conn-2 <<endl;
        check_vpso(connfd);
        close(connfd);
        return -1;
    }else if (bytenum < 0){
        perror("ERROR:Receive error\n");
        check_vpso(connfd);
        close(connfd);
        return -1;
    }else{ 
        buf[bytenum] = '\0'; 
        cout<<"INFO:Received message\n";
        cout<<"===================================================================="<<endl;
        cout<< buf <<endl; 
        cout<<"===================================================================="<<endl;
    }
    if(xml_decode(buf,connfd) >= 0){
        return 0;
    }else{
        return -1;
    }
}

int compare(int a, int b){
    if(a-b == 0) 
        return 1;
    else 
        return 0;
}


int xml_decode(char * xml, int sock_fd){
    try{
        if (NULL == xml){
    		cout<<"ERROR:Empty xml message\n";
    		return -1;
    	}
    	handle = new TiXmlDocument();
    	handle->Parse(xml);
    	TiXmlNode* EnvelopeNode = handle->FirstChild("Envelope");
    	const char * EnvelopeType = EnvelopeNode->ToElement()->Attribute("type");
    	if(EnvelopeType != NULL){
    		if(!strcmp(EnvelopeType,"cregister")){
                //onvif registering
    			onvif_fd = sock_fd;
    			const char* onvif_reply = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"r_cregister\">success</Envelope>";
                                            if( send(onvif_fd, onvif_reply , strlen(onvif_reply), 0) == -1) { 
    			 	perror ( "ERROR:Send error" ); 
    			 	return -1;
    			}
                                            cout<<"INFO: A ONVIF Server found..."<<endl;
                                            return 0;
    		}else if(!strcmp(EnvelopeType,"pregister")){
                //proxy registering
    			proxy_fd = sock_fd;
    			const char* proxy_reply = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"r_pregister\">success</Envelope>";
                                            if( send(proxy_fd, proxy_reply , strlen(proxy_reply), 0) == -1) { 
    			 	perror ( "ERROR:Send error" ); 
    			 	return -1;
    			}
                                            cout<<"INFO: A PROXY Server found..."<<endl;
                                            return 0;
    		}else if(!strcmp(EnvelopeType,"vregister")){
                //video registering
    			video_fd = sock_fd;
    			const char* video_reply = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"r_vregister\">success</Envelope>";
                                            if( send(video_fd, video_reply , strlen(video_reply), 0) == -1) { 
    			 	perror ( "ERROR:Send error" ); 
    			 	return -1;
    			}
                                            cout<<"INFO: A VIDEO Server found..."<<endl;
                                            return 0;
    		}else if(!strcmp(EnvelopeType,"sregister")){
                //storage registering
    			storage_fd = sock_fd;
    			const char* storage_reply = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"r_sregister\">success</Envelope>";
                                            if( send(storage_fd, storage_reply , strlen(storage_reply), 0) == -1) { 
    			 	perror ( "ERROR:Send error" );
    			 	return -1;
    			}
                                            cout<<"INFO: A STORAGE Server found..."<<endl;
                                            return 0;
    		}else if(!strcmp(EnvelopeType,"getrtspuri")&& compare(sock_fd, onvif_fd)){
                //to proxy server 
                                            if(proxy_fd > 0 ){
                                                        if ( send ( proxy_fd, xml , strlen(xml), 0) == -1) { 
    			 	           perror ( "ERROR:Send error" ); 
    			 	           return -1;
    			             }
                                                        return 0;
                                            }else{
                                            cout<<"WARNING:No proxy server connected, the message will send no place...\n";
                                            return 0;
                                            }
    		}else if(!strcmp(EnvelopeType,"r_getrtspuri")&& compare(sock_fd,proxy_fd)){
                //to onvif server 
                                            if(onvif_fd > 0 ){
                                                if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                                	               perror ( "ERROR:Send error" ); 
                                		  return -1;
                                	       }
                                                    return 0;
                                            }else{
                                                cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                                return 0;
                                            }
    		}else if(!strcmp(EnvelopeType,"startstorage")&& compare(sock_fd,onvif_fd)){
    			//to storage server
                                        if(storage_fd > 0 ){
                                            if ( send ( storage_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No storage server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"r_startstorage")&& compare(sock_fd, storage_fd)){
    			//to onvif server
                                        if(onvif_fd > 0 ){
                                            if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"stopstorage")&& compare(sock_fd, onvif_fd)){
    			//to storage server
                                        if(storage_fd > 0 ){
                                            if ( send ( storage_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No storage server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"r_stopstorage")&& compare(sock_fd, storage_fd)){
    			//to onvif server
                                        if(onvif_fd > 0 ){
                                            if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"startdeal")&& compare(sock_fd, onvif_fd)){
    			//to video server
                                        if(video_fd > 0 ){
                                            if ( send ( video_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No video server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"r_startdeal")&& compare(sock_fd, video_fd)){
    			//to onvif server
                                        if(onvif_fd > 0 ){
                                            if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"stopdeal")&& compare(sock_fd, onvif_fd)){
    			//to video server
                                        if(video_fd > 0 ){
                                            if ( send ( video_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No video server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"r_stopdeal")&& compare(sock_fd, video_fd)){
    			//to onvif server
                                        if(onvif_fd > 0 ){
                                            if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else if(!strcmp(EnvelopeType,"warning")&& compare(sock_fd, video_fd)){
    			//to onvif server
                                        if(onvif_fd > 0 ){
                                            if ( send ( onvif_fd, xml , strlen(xml), 0) == -1) { 
                            			 	   perror ( "ERROR:Send error" ); 
                            			 	   return -1;
                            			    }
                                            return 0;
                                        }else{
                                            cout<<"WARNING:No onvif server connected, the message will send no place...\n";
                                            return 0;
                                        }
    		}else{
                cout<<"WARNING:Received a unrecgonized type message\n";
                return -1;
            }
        }
        cout<<"WARNING:Received a unrecgonized message\n";
        return -1;
    }catch(exception &e){
        perror("ERROR:error while decoding xml\n");
        return -1;
    }
}



void check_vpso(int conn_fd){
    if(video_fd == conn_fd){
        video_fd = -1;
    }else if(proxy_fd == conn_fd){
        proxy_fd = -1;
    }else if(storage_fd == conn_fd){
        storage_fd = -1;
    }else if(onvif_fd == conn_fd){
        onvif_fd = -1;
    }
}