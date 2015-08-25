#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>  
#include <errno.h>  
#include <netdb.h>
#include <unistd.h>

# define MAXDATASIZE 1024

# define SERVERIP "127.0.0.1" 
# define SERVERPORT 8000

int main( int argc, char * argv[ ] ) 
{
    char buf[ MAXDATASIZE]; 
    int sockfd, numbytes; 
    struct sockaddr_in server_addr; 

    if ( ( sockfd = socket ( AF_INET , SOCK_STREAM , 0)) == - 1) { 
        perror("socket error"); 
        return 1; 
    } 
    memset ( & server_addr, 0, sizeof(struct sockaddr)); 
    server_addr. sin_family = AF_INET; 
    server_addr. sin_port = htons ( SERVERPORT); 
    server_addr. sin_addr. s_addr = inet_addr( SERVERIP); 
    if ( connect ( sockfd, ( struct sockaddr * ) & server_addr, sizeof( struct sockaddr ) ) == -1) { 
        perror ("connect error"); 
        return 1; 
    } 
    char * buffer1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                        <Envelope type=\"cregister\"> \
                        </Envelope>";
                
    char * buffer2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"getrtspuri\"> \
                        <profile> \
                            <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <deviceip>192.168.1.11</deviceip> \
                            <token>PROFILE_H264_CH0_MAJOR</token> \
                            <sourceuri>rtsp://192.168.101.151:8554/c0xa8x1xcg</sourceuri> \
                        </profile> \
                        <profile> \
                             <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <deviceip>192.168.1.11</deviceip> \
                            <token>PROFILE_H264_CH0_MINOR</token> \
                            <sourceuri>rtsp://192.168.101.151:8554/c0xa8x1xcg</sourceuri> \
                        </profile> \
                    </Envelope>";
        

        
    char * buffer3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"startstorage\"> \
                        <profile> \
                             <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <deviceip>192.168.1.31</deviceip> \
                            <rtspuri>rtsp://192.168.20.253:8554/c0xa8x1x1fg </rtspuri> \
                            <height>720</height> \
                            <width>1280</width> \
                            <split>30</split> \
                            <format>mp4</format> \
                        </profile> \
                    </Envelope>";
                    
    char * buffer4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"stopstorage\"> \
                        <profile> \
                            <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <rtspuri>rtsp://192.168.20.253:8554/c0xa8x1x1fg </rtspuri> \
                        </profile> \
                    </Envelope>";
    char * buffer41 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"available\"> \
                        <profile> \
                            <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                        </profile> \
                    </Envelope>";

    char * buffer5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"startdeal\"> \
                        <profile> \
                            <category>cross</category> \
                             <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                             <rtspuri>rtsp://192.168.101.151:8554/c0xa8x1xcg</rtspuri> \
                    	   <startcol>0</startcol> \
                    	   <startrow>1000</startrow> \
                    	   <endcol>1000</endcol> \
                    	   <endrow>0</endrow> \
                        </profile> \
                    </Envelope>";

    char * buffer6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"stopdeal\"> \
                        <profile> \
                             <mac>4f:8f:xx:xx:xx:xx</mac> \
                            <cfd>6</cfd> \
                            <rtspuri>rtsp://192.168.101.151:8554/c0xa8x1xcg</rtspuri> \
                        </profile> \
                    </Envelope>";

    char * buffera = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"startstorage\"> \
                        <profile> \
                             <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <deviceip>192.168.1.8</deviceip> \
                            <rtspuri>rtsp://192.168.20.253:8554/c0xa8x1x8g</rtspuri> \
                            <height>720</height> \
                            <width>1280</width> \
                            <split>30</split> \
                            <format>mp4</format> \
                        </profile> \
                    </Envelope>";
                    
    char * bufferb = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                    <Envelope type=\"stopstorage\"> \
                        <profile> \
                            <mac>4f:8f:xx:xx:xx:xx</mac> \
                             <cfd>6</cfd> \
                            <rtspuri>rtsp://192.168.20.253:8554/c0xa8x1x8g</rtspuri> \
                        </profile> \
                    </Envelope>";
    int x;
    int flag = 1;
    while(flag){
        printf("1---registercontrol\n2---getrtspuri\n3---startstorage\n4---stopstorage\n5---startdeal\n6---stopdeal\n7---available\n0---quit\nplease choose:\n");
        scanf("%d",&x);
        switch(x){
            case 1:
                if ( send ( sockfd, buffer1 , strlen(buffer1), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                } 
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 2:
                if ( send ( sockfd, buffer2 , strlen(buffer2), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                } 
                printf("send getrtspuri!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return -1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                }
                getchar();
                break;
            case 3:
                if ( send ( sockfd, buffer3 , strlen(buffer3), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send startstorage!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 4:
                if ( send ( sockfd, buffer4 , strlen(buffer4), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send stopstorage!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 5:
                if ( send ( sockfd, buffer5 , strlen(buffer5), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send startdeal!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 6:
                if ( send ( sockfd, buffer6 , strlen(buffer6), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send stopdeal!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 7:
                if ( send ( sockfd, buffer41 , strlen(buffer6), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send available!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 8:
                if ( send ( sockfd, buffera, strlen(buffera), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send startstorage!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 9:
                if ( send ( sockfd, bufferb, strlen(bufferb), 0) == -1) { 
                    perror ( "send error" ); 
                    return 1; 
                }
                printf("send stopstorage!\n");
                if ( ( numbytes = recv ( sockfd, buf, MAXDATASIZE, 0) ) == -1) { 
                    perror( "recv error" ); 
                    return 1; 
                } 
                if ( numbytes) { 
                    buf[numbytes] = '\0'; 
                    printf("received: %s\n",buf); 
                } 
                getchar();
                break;
            case 0:
                flag = 0;
                break;
        }
    }

    close (sockfd); 
    
    return 0; 
}
