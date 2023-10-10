/* 
    Simple DNS proxy server implementation.  
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DNS_PORT 53
#define SMALL_BUFFER_SIZE 64
#define BIG_BUFFER_SIZE 4096
#define DEFAULT_CONFIG_FILE_NAME "dns_proxy_config.txt"
FILE * file = NULL;

char PROXY_SERVER_ADDR[SMALL_BUFFER_SIZE];
char UPSTREAM_DNS[SMALL_BUFFER_SIZE];
char RESTRICTED_DOMAINS[BIG_BUFFER_SIZE];
char RESPONSE_PHRASE[SMALL_BUFFER_SIZE];  

//Function to forward DNS query to the upstream DNS server
void forwardDNSquery(const char * query, char * response, const char * upstreamDNS) 
{
    
    int upstreamDNSsocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (upstreamDNSsocket == -1) 
    {
        perror("Error creating socket for upstream DNS server");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in upstreamDNSaddr;
    upstreamDNSaddr.sin_family = AF_INET;

    upstreamDNSaddr.sin_port = htons(DNS_PORT);
    inet_pton(AF_INET, upstreamDNS, &upstreamDNSaddr.sin_addr);

    // Send the DNS query to the upstream DNS server
    sendto(upstreamDNSsocket, query, strlen(query), 0,
           (struct sockaddr *)&upstreamDNSaddr, sizeof(upstreamDNSaddr));
    
    //for debug and testing
    //puts("before response from the upstream DNS server");
    
    // Receive the response from the upstream DNS server
    ssize_t receivedBytes = recvfrom(upstreamDNSsocket, response, BIG_BUFFER_SIZE, 0, NULL, NULL);
    if (receivedBytes == -1) 
    {
        perror("Error receiving data from upstream DNS server");
        close(upstreamDNSsocket);
        exit(EXIT_FAILURE);
    }
    //for debug and testing
    //puts("after response from the upstream DNS server");

    // Null-terminate the received data
    response[receivedBytes] = '\0';

    close(upstreamDNSsocket);
}

//Function for parsing the config file
void parseConfigFile(FILE * restrict fl, char * proxyServer, char * upstreamDNS, char * restrictedDomains, char * response)
{   
    
    fscanf(fl, "%64s\n", proxyServer);
    fscanf(fl, "%64s\n", upstreamDNS);
    fscanf(fl, "%4096s\n", restrictedDomains);  //Probably I need to change that line
    fscanf(fl, "%[^\n]\n", response);

    //code for debugging
    //printf("PROXY_SERVER_ADDR: %s\n", proxyServer); 
    //printf("UPSTREAM_DNS: %s\n", upstreamDNS); 
    //printf("RESTRICTED_DOMAINS: %s\n", restrictedDomains); 
    //printf("RESPONSE_PHRASE: %s\n", response);  
    
}


int main(int argc, char * argv[])
{   

    //if config file name is not provided as a command line argument, the server opens the default config file
    //if a command line argument is provided, the program considers it the name of the config file
    if(argc == 1)
    {  
        file = fopen(DEFAULT_CONFIG_FILE_NAME, "r");
        if (file == NULL) 
        {
            perror("Error opening file");
            return 1; 
        }
    }
    else if (argc == 2)
    {   
        file = fopen(argv[1], "r");
        if (file == NULL) 
        {
            perror("Error opening file");
            return 1; 
        }        
    }
    else
    {
        fprintf(stderr, "Usage: %s <filename>\nUsage: %s\n", argv[0], argv[0]);
        return 1; 
    }
    
    parseConfigFile(file, PROXY_SERVER_ADDR, UPSTREAM_DNS, RESTRICTED_DOMAINS, RESPONSE_PHRASE); 
    
    // Create server socket
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket == -1) 
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind to address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(PROXY_SERVER_ADDR); 
    if (bind(serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) 
    {
        perror("Error binding socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }       

    printf("DNS Proxy Server listening on %s\n", PROXY_SERVER_ADDR);
    
    while (1) 
    {
        char clientRequest[BIG_BUFFER_SIZE];
        char serverResponse[BIG_BUFFER_SIZE];

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Receive DNS query from client
        ssize_t receivedBytes = recvfrom(serverSocket, clientRequest, sizeof(clientRequest), 0,
                                         (struct sockaddr *)&client_addr, &addr_len);
        
        //for debug and testing
        //printf("receivedBytes = %ld\n", receivedBytes);

        if (receivedBytes == -1) 
        {
            perror("Error receiving data");
            continue;
        }

        // Null-terminate the received data
        clientRequest[receivedBytes] = '\0';

        // Check if the domain is restricted
        if (strstr(RESTRICTED_DOMAINS, clientRequest) != NULL) 
        {
            snprintf(serverResponse, BIG_BUFFER_SIZE, "%s", RESPONSE_PHRASE);
        } 
        else 
        {        
            //for debug and testing
            //printf("befor forwarding\n");

            // Forward the query to the upstream DNS server for resolution
            forwardDNSquery(clientRequest, serverResponse, UPSTREAM_DNS);

            //for debug and testing
            //printf("after forwarding\n");
        }
        
        //for debug and testing
        //printf("befor sending response to the client\n");

        // Send DNS response back to the client
        sendto(serverSocket, serverResponse, strlen(serverResponse), 0,
               (struct sockaddr *)&client_addr, addr_len);
        
        //for debug and testing
        //printf("after sending response to the client\n");

    }    
    
    fclose(file); 
    close(serverSocket);       
    return 0;
}