#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

char *CONFIG_FILE = "httpd.conf";
char PORT[100];
char HOST[100];
char WWWROOT[100];

struct data {
    char buff[1000];
    int sock;
    char method[100];
    char http[100];
    char url[100];
    char filename[100];
    char path[100];
};

void init(){

    FILE *fp = fopen(CONFIG_FILE, "r");
    if(fp==NULL){
        printf("Error in file\n");
        exit(0);
    }
    char line[1000];    
    char key[100], value[100];

    while((fgets(line, 1000, fp))!=NULL){
        printf("%s\n", line);
        sscanf(line, "%s %s", key, value);
        if(strcmp(key, "port")==0){
            strcpy(PORT, value);
        }
        else if(strcmp(key, "host")==0){
            strcpy(HOST, value);
        }
        else if(strcmp(key, "wwwroot")==0){
            strcpy(WWWROOT, value);
        }
        else{
            printf("Corrupt file\n");
            exit(0);
        }
    }
}

void read_input(int argc, char *argv[]){
    int i;
    for(i=1; i<argc; i++){
        if(strcmp(argv[i], "-p")==0){
            strcpy(PORT, argv[++i]);
        }
        else if(strcmp(argv[i], "-h")==0){
            strcpy(HOST, argv[++i]);
        }
        else {
            printf("Wrong input parameter\n");
            exit(0);
        }

    }
}

int get_status(char *method, char *http, char *url, char *filename, char *buff){

    char *token;
    char *temp = strdup(buff); 

    while((token = strsep(&temp, "\n"))!=NULL){
       sscanf(token, "%[^ ] %[^ ] %s", method, url, http); 
       char *temp_f = strrchr(url, '/')+1;
       strcpy(filename, temp_f);
       return 0;
    }
}

int checkMethod(char *method){
    if(strcmp(method, "GET")==0 || strcmp(method, "HEAD")==0) return 0;
    return -1;
}

int get_extension(char *filename, char *ext){
    strcpy(ext, strrchr(filename, '.')+1);
    return 0;
}

int get_mime(char *ext, char *mime){
    FILE *fp = fopen("mime.types", "r");
    if(fp==NULL) return -1;
    char line[100], key[100], value[100];

    while((fgets(line, 100, fp))!=NULL){
       if(line[0] == '#') continue;
       sscanf(line, "%s %s", key, value);
       if(strcmp(value, "html")==0){
           strcpy(mime, key); 
           return 0;
       }
    }
   
    return -1;
}

int get_size(char *path){
    FILE *fp = fopen(path, "r");
    if(fp==NULL) return -1;

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);
    return size;
}

void sendFile(char *path, int sock){

    FILE *fp = fopen(path, "r");
    char ch;

    while((ch=fgetc(fp))!=EOF){
        send(sock, &ch, sizeof(ch), 0);
    }
}



void send_response_header(char *buff, char *size, char *mime, int sock){

    char message[1000];
    char *content_head = "Content-Type: ";
    char *size_head = "Content-Length: ";
    char *date_head = "Date: ";
    
    time_t rawtime;
    time(&rawtime);

    strcat(message, buff);
    strcat(message, "\r\n");
    strcat(message, content_head );
    strcat(message, mime);
    strcat(message, "\r\n");
    strcat(message, size_head);
    strcat(message, size); 
    strcat(message, "\r\n");
    strcat(message, date_head);
    strcat(message, (char *)ctime(&rawtime));
    
    strcat(message, "\r\n");

    printf("Response\n%s", message);
    send(sock, message, strlen(message), 0);

}

void *run_thread(void *attr){
   struct data *d = (struct data *)attr;
   printf("%s\n",d->buff);  

   //Find the status line data
   if(get_status(d->method, d->http, d->url, d->filename, d->buff) == -1){
       printf("ERROR: STATUS ERROR\n");
       //Send Bad request
       exit(0);
   }


   //Check the method type
   if(checkMethod(d->method)==-1){
       printf("ERROR: STATUS ERROR\n");
       //Send Bad request
       exit(0);
   }

   char ext[100];
   get_extension(d->filename, ext);

   char mime[100];
   if(get_mime(ext, mime)==-1){
       printf("ERROR: MIME\n");
       //Send Bad request
       close(d->sock);
       exit(0);
   }

   //Make path
   strcat(d->path, WWWROOT);
   strcat(d->path, d->filename);

   //Get the size of the file
   int s;
   if((s = get_size(d->path))==-1){
       printf("ERROT: SIZE\n");
       // File Not Found
       close(d->sock);
       exit(0);
   }

   printf("%d\n", s);

   //Send the header
   char size[100];
   sprintf(size, "%d", s);
   send_response_header("\r\nHTTP/1.1 200 OK", size, mime, d->sock);

   
   //Open the file

   //Send the file
  sendFile(d->path, d->sock);

}

void print_init(){
    printf("HOST: %s\n", HOST);
    printf("PORT: %s\n", PORT);
    printf("........................\n");
}

void start(){
    // Socket Connection
    struct addrinfo hints, *res;
    char buff[1000];

    memset(&hints, 0, 1000);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if((getaddrinfo(HOST, PORT, &hints, &res)) != 0){
        perror("GetAddrInfo");
        exit(0);
    }

    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if(sock==-1){
        perror("Socket");
        exit(0);
    }

    if(bind(sock, res->ai_addr, res->ai_addrlen)==-1){
        perror("Bind");
        exit(0);
    }

    if(listen(sock, 10)==-1){
        perror("Listen");
        exit(0);
     }

    while(1){
        printf(".....Waiting..........\n");
        int new_sock = accept(sock, res->ai_addr, &(res->ai_addrlen));
        if(new_sock==-1){
            perror("Accept");
        }
        printf(".........Connected..........\n");

        recv(new_sock, buff, 1000, 0);

        //printf("OP: %s\n", buff);
        pthread_t th;
        struct data *d = (struct data *)malloc(sizeof(*d));
        strcpy(d->buff, buff);
        d->sock = new_sock;
 
        pthread_create(&th, NULL, run_thread, (void*)d);

        //close(new_sock);

    }
}



int main(int argc, char *argv[]){
    init();
    read_input(argc, argv);
    print_init();
    start();
}
