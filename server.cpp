#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <map>

using namespace std;

const int PORT = 9916;

int sock;
int kq;
struct kevent event;

map<int, int> clients;

bool setup_socket(){
    sockaddr_in addr;
    
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if( sock == 0 ){
        printf("socket error");
        return false;
    }
    
    int on=1;
    setsockopt(sock,SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
               
    memset( &addr, 0, sizeof(addr) );
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr = htonl( INADDR_ANY );
    addr.sin_port = htons( PORT );
    
    /* bind */
    if( ::bind( sock, (sockaddr*)&addr, sizeof(addr) ) == -1 ){
        printf("bind error");
        return false;
    }
    
    /* listen */
    if( listen( sock, 5 ) == -1 ){
        printf("listen error");
        return false;
    }
    
    return true;
}
bool setup_kqueue(){
    kq = kqueue();
    
    EV_SET(
           &event, sock,
           EVFILT_READ, EV_ADD | EV_ENABLE,
           0,0,0 );
    kevent( kq, &event, 1, 0,0,0 );
    
    return true;
}

int on_accept(){
    sockaddr addr;
    socklen_t addrLen;
    int client;
    
    client = accept( sock, &addr, &addrLen );
    
    return client;
}

void send_cls(int sock){
    char msg[128];
    sprintf(msg, "%c[2J",0x1B);
    write(sock, msg, strlen(msg));
}
void send_gotoxy(int sock, 	int x,int y){
    char msg[128];
    sprintf(msg, "%c[%d;%df",0x1B,y,x);
    write(sock, msg, strlen(msg));
}
void send_lineclear(int sock){
    char msg[128];
    sprintf(msg, "%c[0K",0x1B);
    write(sock, msg, strlen(msg));
}
void reset_input(int sock){
    send_gotoxy(sock, 0,22);
    write( sock, "msg : ", strlen("msg : "));
    send_lineclear(sock);
}

void event_loop(){
    
    while( true ){
        struct kevent events[1024];
        
        int nEvents = kevent( kq, 0,0, events, 1024, 0 );
        
        for(int i=0;i<nEvents;i++){
            struct kevent &ev = events[i];
            
            if( ev.flags & EV_ERROR ){
                printf("something went wrong\n");
                continue;
            }
            
            if( ev.ident == sock ){
                if( ev.filter == EVFILT_READ ){
                    int client = on_accept();
                    
                    printf("accepted %d\n", client);
                    
                    EV_SET(
                           &event, client,
                           EVFILT_READ, EV_ADD,
                           0,0,0 );
                    kevent( kq, &event, 1, 0,0,0 );
                    
                    clients[client] = 1;
                    
                    send_cls( client );
                    reset_input( client );
                }
            }
            else{
                char buf[1024];
                int len;
                
                len = read( ev.ident, buf, 1024 );
                
                /* 연결 종료됨 */
                if( len <= 0 || ev.flags & EV_EOF ){
                    EV_SET(
                           &event, ev.ident,
                           EVFILT_READ, EV_DELETE,
                           0,0,0 );
                    
                    kevent( kq, &event, 1, 0,0,0 );
                    
                    clients.erase( ev.ident );
                    printf("connection closed %d\n", ev.ident);
                }
                /* 데이터 수신함 */
                else if( ev.flags & EVFILT_READ ){
                    for(auto &pair : clients){
                        int client = pair.first;
                        int &y = pair.second;
                        
                        if(y >= 21){
                            send_cls(client);
                            y = 1;
                        }
                        
                        send_gotoxy(client, 3,y++);
                        write( client, buf, len );
                        reset_input( client );
                    }
                }
            }
            
        }
    }
}

int main(){
    setup_socket();
    setup_kqueue();
    
    printf("running\n");
    
    event_loop();
    
    return 0;
}
