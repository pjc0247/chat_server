#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <set>

using namespace std;

const int PORT = 9916;

int sock;
int kq;
struct kevent event;

set<int> clients;

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
                    
                    clients.insert( client );
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
                    for(auto client : clients)
                        write( client, buf, len );
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
