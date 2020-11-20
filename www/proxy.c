#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void *thread(void *vargp);
char* itoa(int value, char* result, int base);
int dnslookup(const char* hostname, char* firstIPstr, int maxSize);
void * test(void * vargp);
int hostname_to_ip(char *hostname , char *ip);
int build_server_addr(struct sockaddr_in* serv_addr, char * ip_add);
struct Thread_object{
    pthread_mutex_t* file_lock;
    int* connfdp;
};

int main(int argc, char **argv) 
{
    int listenfd, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 
    pthread_mutex_t* file_lock;
    file_lock = malloc(sizeof(pthread_mutex_t));
   /* char* ip_add = malloc(sizeof(char)*MAXBUF);
    //ip_add = gethostbyname("netsys.cs.colorado.edu/");
    hostname_to_ip("netsys.cs.colorado.edu", ip_add);
    printf("resolved : %s", ip_add);*/

    //printf("%s\n",ip_add );
   /* cached = fopen("cached_domains.txt", "w");
    if(cached == NULL){
        printf("File not found!\n");
    }*/

    //initialize file_lock
    if (pthread_mutex_init(file_lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    } 

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);

    //initialize thread_object
    struct Thread_object thread_object;
    thread_object.file_lock = file_lock;
   /* thread_object.cached = fopen("cached_domains.txt", "wa");
    fseek(thread_object.cached, 0, SEEK_END);
    fputs("Hey testjfkljlking", thread_object.cached);
    fclose(thread_object.cached);*/
    printf("Listening on port %d\n", port);
    while (1) {    
        //connfdp = malloc(sizeof(int));
        //thread_object.connfdp = connfdp;
        thread_object.connfdp = (intptr_t*)accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, (void *)&thread_object);
        //pthread_create(&tid, NULL, test, (void *)&thread_object);
    }//while(1)
}//main






   /* 
    char * resolved_name = malloc(sizeof(char)*MAXBUF);
    hostname_to_ip("google.com", resolved_name);
    printf("%s\n",resolved_name );

   while (1) {    
    //prepare thread object to be sent to thread
    struct Thread_object thread_obj;
    thread_obj.ip_addr = (char *)malloc(sizeof(char) *100);
    thread_obj.connfdp = malloc(sizeof(int));
    thread_obj.connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);

    pthread_create(&tid, NULL, thread, (void *)&thread_obj);
    }*/


/*void * test(void * vargp) 
{  

    struct Thread_object *thread_object;
    thread_object = vargp;
    int connfd = thread_object->connfdp;
    printf("In test func\n");
        char* request = malloc(sizeof(char)*MAXBUF);
    pthread_detach(pthread_self()); 
    size_t n;
    size_t m;
    n = read(connfd, request, MAXLINE);
    printf("read %d bytes \n",n);
    printf("request is %s\n",request);
    char* resolved_name = malloc(sizeof(char)*MAXBUF);
    resolved_name = "HTTP/1.1 400 Internal Server Error\r\nContent-Type:text/plain\r\nContent-Length:10\r\n\r\n<p>Does this print</p>";

    m = write(connfd, resolved_name, sizeof(char)*strlen(resolved_name));

    printf("worte %d bytes\n",m);
    return NULL;
}*/

/* thread routine */
void * thread(void * vargp) 
{  
    //pointer to struct
    struct Thread_object *thread_object;

    //thread_object is pointer to thread_obj
    thread_object = (struct Thread_object*)vargp;

    int connfd = (int)thread_object->connfdp;

    pthread_detach(pthread_self()); 
    

    //various string vars needed. All declared with malloc so they go on heap. 
    //char* ip_add = malloc(sizeof(char)*MAXBUF);
    char* resolved_name = malloc(sizeof(char)*MAXBUF);  
    char* request = malloc(sizeof(char)*MAXBUF);
    char* request_type = malloc(sizeof(char)*MAXBUF);
    char* http_packet = malloc(sizeof(char) *MAXBUF);
    //char* request_header_token = malloc(sizeof(char) *MAXBUF);
    char* domain_name = malloc(sizeof(char)*MAXBUF);
    char* host_name = malloc(sizeof(char)*MAXBUF);
    char* http_response = malloc(sizeof(char)*MAXBUF);
    char* http_response_copy = malloc(sizeof(char)*MAXBUF);
    FILE* fp;
    int n;
    n = read(connfd, request, MAXLINE);

    if(n < 0){
        printf("Bad connection\n");
    }

    //copy packet info before parsing
    strcpy(http_packet,request);

    
    //gets first element of http packet
    request_type = strtok(request, " ");      
    //nested if to check if GET request 
    if(strcmp(request_type, "GET") == 0){
        
        //gets next element in http request
        domain_name = strtok(NULL, " ");
        //sscanf(domain_name, "http://%[^/]", host_name);
        sscanf(domain_name, "%*[^/]%*[/]%[^/]", host_name);
        printf("domain_name is %s\n",domain_name );
        printf("host name is %s\n",host_name );


        //free old pointers
        /*free(request);
        free(domain_name);
        free(request_type);*/
        //---- ---CRITICAL SECTION------ lock is good in if and else
        pthread_mutex_lock(thread_object->file_lock);
        fp = fopen(host_name, "r");
        
        //if host_name is not saved as a file, we have to do dns lookup
        if( fp == NULL){
            printf("File not found\n");

            int dns_ret = hostname_to_ip(host_name, resolved_name);

            printf("%s\n",resolved_name);
            if(dns_ret == 0 ){
                printf("Saving address to file\n");
            
                fp = fopen(host_name, "w");

                fputs(resolved_name, fp);
                fputs("\n", fp);

                fclose(fp);
                pthread_mutex_unlock(thread_object->file_lock);
            //---------------critical section --------------------
            }//dns_ret if
        }//do dns lookup if
        //file exists
        else{
            fscanf(fp, "%s", resolved_name);
            printf("resolved from file is %s\n", resolved_name);
            fclose(fp);
            pthread_mutex_unlock(thread_object->file_lock);

        }


        //DONE WITH DNS CACHE
        
        struct sockaddr_in* serv_addr = malloc(sizeof(struct sockaddr_in));
        int sock;
        int check_addr; 

        //takes pointer to sockaddr_in and ip_addr as string
        check_addr = build_server_addr(serv_addr, resolved_name);

        if(check_addr < 0){
            printf("Error building address\n");
            return NULL;
        }
        else{
            if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
            { 
                printf("\n Socket creation error \n"); 
                return NULL; 
            } 
            if(connect(sock, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0) 
                { 
                    printf("\nConnection Failed \n"); 
                    return NULL; 
                }
            int n;
            n = write(sock, http_packet, sizeof(char)*strlen(http_packet));
            if(n<0){
                printf("Error writing\n");
                return NULL;
            }
            printf("wrote %d bytes\n", n);
            int bytes_read;
            bytes_read = read(sock, http_response, sizeof(char)*MAXBUF);
           /* while(*ret = read(sock, http_response, sizeof(char)*MAXBUF) >0){
                printf("read %d bytes\n",*ret);
                *i = *i + 1;
                printf("value of i is %d\n",*i);
                printf("RESPONSE IS:\n%s\n",http_response);
                strcpy(http_response,http_response_copy);
                bzero(http_response, sizeof(char)*MAXBUF);
            }//while*/

            printf("read %d bytes\n",bytes_read);
            int m;
            m = write(connfd, http_response, sizeof(char)*strlen(http_response));
            printf("wrote %d bytes\n",m);
            //write(connfd, http_response, sizeof(char)*strlen(http_response));
            return NULL;        
    }//if address was built right else

    }//nested if

    //not a GET request, bye
    else{
        printf("Proxy only handles GET. Sorry\n");
        free(resolved_name);
        free(request);
        free(http_packet);
        free(domain_name);
        free(host_name);
        free(http_response);
        free(http_response_copy);

        return NULL;
    }//terminating else
  }//thread  

/*


 

HELPER FUNCTIONS BELOW

 


 */

int build_server_addr(struct sockaddr_in* serv_addr, char * ip_add){
    printf("Building address\n");
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(80);

    //https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    //converts IP to correct format
    if(inet_aton(ip_add, (struct in_addr* )&serv_addr->sin_addr.s_addr) ==0){
        return -1;
    }
    return 1;
}


//https://www.geeksforgeeks.org/socket-programming-cc/
//creates socket to send stuff over internet. 
/*int create_send_socket(char* ip_add, char* http_packet)
{

    struct sockaddr_in serv_addr;
    int sock = 0, valread;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    }//if
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(80);

    //https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    //converts IP to correct format
    inet_aton(ip_add, &serv_addr.sin_addr.s_addr);

    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0){
        printf("\nConnection failed\n");
        return -1;

    }


}//create_send_socket*/



int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */



/*int socket_connect(char *host, in_port_t port){
    struct hostent *hp;
    struct sockaddr_in addr;
    int on = 1, sock;     

    if((hp = gethostbyname(host)) == NULL){
        herror("gethostbyname");
        exit(1);
    }
    copy(hp->h_addr, &addr.sin_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

    if(sock == -1){
        perror("setsockopt");
        exit(1);
    }
    
    if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
        perror("connect");
        exit(1);

    }
    return sock;
}*/

//https://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
int hostname_to_ip(char *hostname , char *ip)
{
    //int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ( (rv = getaddrinfo( hostname , "http" , &hints , &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        h = (struct sockaddr_in *) p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
    
    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}

