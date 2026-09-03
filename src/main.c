#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>

int main()
{
   int server_id=socket(AF_INET,SOCK_STREAM,0);
   if (server_id<0)
   {
      perror("Socket");
      return 1;
   }
   struct sockaddr_in addr;
   {
      memset(&addr,0,sizeof(addr));
      addr.sin_family=AF_INET;
      addr.sin_addr.s_addr=INADDR_ANY;
      addr.sin_port=htons(6379);
   };
   if (bind(server_id,(struct sockaddr *)&addr,sizeof(addr))<0)
   {
      perror("bind");
      return 1;
   }
   if(listen(server_id,5)<0)
   {
      perror("LISTEN");
      return 1;
   }
   printf("Listening on port 6379......\n");
   while (1)
   {
         int client_fd=accept(server_id,NULL,NULL);
         if(client_fd<0)
         {
            perror("ACCEPT");
            return -1;
         }
         printf("Client Connected !! \n");
         char buffer[1024];
         int bytes_read =read(client_fd,buffer,1024);
         buffer[bytes_read]='\0';
         printf("Receiced: %s\n",buffer);
         write(client_fd,"Hello from server \n",19);
         close(client_fd);
   }
   close(server_id);
   return 0;
}