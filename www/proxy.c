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

    printf("Listening on port %d\n", port);
    while (1) {    
        thread_object.connfdp = (intptr_t*)accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, (void *)&thread_object);
    }//while(1)
}//main

/* thread subroutine */
void * thread(void * vargp) 
{  
    //pointer to struct
    struct Thread_object *thread_object;

    //thread_object is pointer to thread_obj
    thread_object = (struct Thread_object*)vargp;

    int connfd = (int)thread_object->connfdp;

    pthread_detach(pthread_self());     

    //various string vars needed. All declared with malloc so they go on heap. 
    char* resolved_name = malloc(sizeof(char)*MAXBUF);  
    char* url_raw = malloc(sizeof(char)*MAXBUF);  
    char* save_ptr = malloc(sizeof(char)*MAXBUF); 
    char* save_ptr2 = malloc(sizeof(char)*MAXBUF); 
    char* request = malloc(sizeof(char)*MAXBUF);
    char* request_type = malloc(sizeof(char)*MAXBUF);
    char* http_packet = malloc(sizeof(char) *MAXBUF);
    char* request_header = malloc(sizeof(char) *MAXBUF);
    char* host_name = malloc(sizeof(char)*MAXBUF);
    char* http_response = malloc(sizeof(char)*MAXBUF);
    char* http_response_copy = malloc(sizeof(char)*MAXBUF);
    char* ip = malloc(sizeof(char)*MAXBUF);
    char* page = malloc(sizeof(char)*MAXBUF);
    char* content_type = malloc(sizeof(char)*MAXBUF);
    FILE* fp;
    int n;
    int* port;
    int succ_parsing;
    n = read(connfd, request, MAXLINE);

    if(n < 0){
        printf("Bad connection\n");
        pthread_exit(pthread_self);
        close(connfd);
        return NULL;
    }

    sscanf(request, "%[^\n]", request_header);
    sscanf(request, "%s", request_type);

    memset(http_packet, 0, strlen(http_packet)); 
    strcpy(http_packet,request);
   
    //nested if to check if GET request 
    if(strcmp(request_type, "GET") == 0){              
        
        //got touse twice to get second element seperated by space
        strtok_r(request," ", &save_ptr); 
        url_raw = strtok_r(NULL," ", &save_ptr);

        //https://stackoverflow.com/questions/726122/best-ways-of-parsing-a-url-using-c
        //if (sscanf(url_raw, "http://%99[^:]:%i/%199[^\n]", ip, &port, page) == 3) { succ_parsing = 1;}
        if (sscanf(url_raw, "http://%99[^/]/%199[^\n]", ip, page) == 2) { succ_parsing = 1;}
        else if (sscanf(url_raw, "http://%99[^:]:%i[^\n]", ip, &port) == 2) { succ_parsing = 1;}
        else if (sscanf(url_raw, "http://%99[^/]", ip) == 1) { succ_parsing = 1;}
        else{ printf("Error parsing URL\n"); return NULL;}


        //sscanf(request_header, "http://%99%", domain_name);
        //printf("domain name is %s port is %d page is %s \n", ip, port, page );
        //sscanf(request_header, "%*[^/]%*[/]%[^/]", host_name);
        //printf("hostname is %s\n",host_name );

        //---- ---CRITICAL SECTION------ lock is good in if and else
        pthread_mutex_lock(thread_object->file_lock);
        fp = fopen(host_name, "r");
        
        //if host_name is not saved as a file, we have to do dns lookup
        if( fp == NULL){
            printf("File not found\n");

            int dns_ret = hostname_to_ip(ip, resolved_name);

            printf("resolved name is %s\n",resolved_name);

            if(dns_ret == 0 ){
                printf("Saving address to file\n");
            
                fp = fopen(ip, "w");

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
        printf("resolved_name is %s\n", resolved_name);
        check_addr = build_server_addr(serv_addr, resolved_name);

        if(check_addr < 0){
            printf("Error building address\n");
            pthread_exit(pthread_self);
            return NULL;
        }
        else{
            if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
            { 
                printf("\n Socket creation error \n"); 
                pthread_exit(pthread_self);
                return NULL; 
            } 
            if(connect(sock, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0) 
                { 
                    printf("\nConnection Failed \n"); 
                    pthread_exit(pthread_self);
                    return NULL; 
                }
            int n;

            n = write(sock, http_packet, strlen(http_packet));
            if(n<0){
                printf("Error writing\n");
                close(sock);
                close(connfd);
                pthread_exit(pthread_self);
                return NULL;
            }
            printf("wrote %d bytes to server\n", n);

            //if page has a dot in it
            if(strstr(page, ".") != NULL){
                content_type = strtok_r(page, ".", &save_ptr2);
            }
            if(content_type !=NULL){
                content_type = strtok_r(NULL, ".",&save_ptr2);
            }
        
            while(1){
                int bytes_read;
                int m;
                //memset(http_response, 0, MAXBUF); 
                bytes_read = read(sock, http_response, MAXBUF);
                if(bytes_read < 0){
                    printf("Error reading from network socket.\n");
                    close(sock);
                    close(connfd);
                    pthread_exit(pthread_self);
                    return NULL;
                }
                printf("Read %d bytes from server\n", bytes_read);
                //need to parse http_response and look for content type,
                //if its an image, handle accordingly. 
                if(bytes_read == 0){                
                    //exit loop
                    printf("Exiting program\n");
                    
                    break;
                }
               /* if(content_type != NULL){
                    if(     strcmp(content_type, "png") == 0 ||
                            strcmp(content_type, "jpg") == 0 ||
                            strcmp(content_type, "ico") == 0 ||
                            strcmp(content_type, "gif") == 0 
                    ){
                        printf("Handling image\n");
                        m = write(connfd, http_response, bytes_read);
                        //m = write(connfd, http_response, bytes_read - strlen(http_response));
                        printf("wrote %d bytes back to client\n",m);
                    }    

                }//content type not null */      
                if(bytes_read> 0){
                    m = write(connfd, http_response, bytes_read);
                    if(m < 0){
                        printf("Error writing back to client\n");
                        close(sock);
                        close(connfd);
                        pthread_exit(pthread_self);
                        return NULL;
                    }
                    printf("wrote %d bytes back to client\n",m);
                }


            }//forever while
        free(resolved_name);
        free(request);
        free(http_packet);
        free(host_name);
        free(http_response);
        close(connfd);
        pthread_exit(pthread_self);
        return NULL;        
    }//if address was built right else

    }//nested if

    //not a GET request, bye
    else{
        printf("Proxy only handles GET. Sorry\n");
        free(resolved_name);
        free(request);
        free(http_packet);
        free(host_name);
        free(http_response);
        close(connfd);
        pthread_exit(pthread_self);
        return NULL;
    }//terminating else
  }//thread  

/*


 

HELPER FUNCTIONS BELOW  





*/
int build_server_addr(struct sockaddr_in* serv_addr, char * ip_add){
    printf("Building address\n");
    serv_addr->sin_family = AF_INET; //ipV4
    serv_addr->sin_port = htons(80);

    //https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    //converts IP to correct format
    if(inet_aton(ip_add, (struct in_addr* )&serv_addr->sin_addr.s_addr) ==0){
        return -1;
    }
    printf("Address built successfully\n");
    return 1;
}

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

