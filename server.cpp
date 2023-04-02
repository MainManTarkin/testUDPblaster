#include <iostream>
#include <string>

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include <getopt.h>
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>

#include "defaults.hpp"
#include "structure.hpp"

bool serverDebugValue = false;

const char serverHelpText[] =
    "|---------------------------------------------------|\n"
    "|====================== Help =======================|\n"
    "|    -h - prints out program arg contents           |\n"
    "|    -p - defines port number other then defualt    |\n"
    "|         39390                                     |\n"
    "|    -d - activate debugging info                   |\n"
    "|---------------------------------------------------|\n";

struct argHolder
{

    argHolder() = default;

    // booleans used to tell the argProcess what to put into struct serverContainer
    bool helpArg = false;
    bool portRedefineArg = false;

    // holds new port name
    char *argParamValue = nullptr;
};

struct serverInfo
{

    serverInfo() = default;

    int ipHostAddr = 0; // defualt 0 means localhost

    char *bindingPort = nullptr;
};

struct serverContainer
{

    serverContainer() = default;

    struct serverInfo *serverInfoHolder = nullptr;

    // sockets listener and talker
    int bindedListenSocket = 0;
    int connectedClientSocket = 0;

    // address structs
    struct addrinfo basedInfo;
    struct addrinfo *serverAddrInfo = nullptr;
    struct addrinfo *peerList = nullptr;

    // client List
    struct sockaddr_storage connectedClientList;

    // socket length
    socklen_t addressLength = sizeof(struct sockaddr_storage);

    // host name store
    char serverHostname[255];

    // optval
    int optionValue = 1;
};

// handles program args
inline int argHandler(int argc, char *argv[], struct argHolder *argHolderInput)
{

    int argHandlerRetVal = 0;
    int optRetVal = 0;

    while ((optRetVal = getopt(argc, argv, "hdp:")) != -1 && argHandlerRetVal == 0)
    {

        switch (optRetVal)
        {
        case 'h':

            argHolderInput->helpArg = true;

            break;
        case 'p':
            argHolderInput->portRedefineArg = true;
            argHolderInput->argParamValue = new char(strlen(optarg));

            if (argHolderInput->argParamValue == nullptr)
            {

                std::cerr << "argHandler() - error acquiring memory with new for ->argParamValue" << std::endl;
                argHandlerRetVal = 1;
                break;
            }

            strcpy(argHolderInput->argParamValue, optarg);

            break;
        case 'd':

            serverDebugValue = true;

            break;
        default:
            // if any defualt case occurs it is an error so set return value to one end loop and return
            argHandlerRetVal = 1;

            std::cerr << "argHandler() - ran into defualt case: " << std::endl;

            // clean up possible allocation
            if (argHolderInput->argParamValue != nullptr)
            {

                delete argHolderInput->argParamValue;
            }

            break;
        }
    }

    return argHandlerRetVal;
}

inline int argProcessor(struct argHolder *argHolderInput, struct serverInfo *serverInfoInput)
{

    int argProcessorRetVal = 0;

    // if the -p option is ever given then this wont be used
    const char defualtPortNumber[] = "39390";

    // look at the list of booleans in argHolderInput
    // if any of them are true then add that to the client struct

    if (argHolderInput->helpArg)
    {

        std::cout << serverHelpText << std::endl;

        argProcessorRetVal = 1;
    }

    if (argHolderInput->portRedefineArg)
    {

        serverInfoInput->bindingPort = argHolderInput->argParamValue;
    }
    else
    {

        serverInfoInput->bindingPort = new char(sizeof(defualtPortNumber));

        if (serverInfoInput->bindingPort == nullptr)
        {

            std::cerr << "argProcessor() - problem with new allocation to ->bindingport" << std::endl;

            argProcessorRetVal = 1;
        }
        else
        {

            strcpy(serverInfoInput->bindingPort, defualtPortNumber);
        }
    }

    return argProcessorRetVal;
}

void *getSocketAddress(struct sockaddr *socketAddInput)
{
    if (socketAddInput->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)socketAddInput)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)socketAddInput)->sin6_addr);
}

inline int prepareServerInfo(struct serverInfo *serverInfoInput, struct serverContainer *serverContainerInput)
{

    int prepareServerInfoRetVal = 0;

    serverContainerInput->serverInfoHolder = serverInfoInput;

    memset(&serverContainerInput->serverHostname, 0, sizeof(serverContainerInput->serverHostname));
    memset(&serverContainerInput->basedInfo, 0, sizeof(struct addrinfo));

    // get hostname and print it for debug mode

    if (serverDebugValue)
    {

        if (gethostname(serverContainerInput->serverHostname, sizeof(serverContainerInput->serverHostname)))
        {

            perror("prepareServerInfo() - error with gethostname(): ");

            prepareServerInfoRetVal = 1;
        }else{

            std::cout << "Server hostname: " << serverContainerInput->serverHostname << std::endl;

        }
    }

    // prepare the base server addrinfo for getting host address

    if (!prepareServerInfoRetVal)
    {
        serverContainerInput->basedInfo.ai_family = AF_UNSPEC;
        serverContainerInput->basedInfo.ai_socktype = SOCK_DGRAM;
        serverContainerInput->basedInfo.ai_flags = AI_PASSIVE;

        if ((prepareServerInfoRetVal = getaddrinfo(NULL, serverInfoInput->bindingPort, &serverContainerInput->basedInfo, &serverContainerInput->serverAddrInfo)) != 0)
        {

            fprintf(stderr, "prepareServerInfo() - problem with getaddrinfo: %s\n", gai_strerror(prepareServerInfoRetVal));
            prepareServerInfoRetVal = 1;
        }
    }

    // began binding and socket creation

    if (!prepareServerInfoRetVal)
    {
        for (serverContainerInput->peerList = serverContainerInput->serverAddrInfo; serverContainerInput->peerList != nullptr; serverContainerInput->peerList = serverContainerInput->peerList->ai_next)
        {

            if ((serverContainerInput->bindedListenSocket = socket(serverContainerInput->peerList->ai_family, serverContainerInput->peerList->ai_family, serverContainerInput->peerList->ai_protocol)) == -1)
            {

                perror("prepareServerInfo() - problem with socket() creation: ");
                continue;
            }

            if (setsockopt(serverContainerInput->bindedListenSocket, SOL_SOCKET, SO_REUSEADDR, &serverContainerInput->optionValue, sizeof(int)) == -1)
            {

                perror("prepareServerInfo() - problem with setsockopt(): ");
                prepareServerInfoRetVal = 1;
                break;
            }

            if (bind(serverContainerInput->bindedListenSocket, serverContainerInput->peerList->ai_addr, serverContainerInput->peerList->ai_addrlen) == -1)
            {

                if (close(serverContainerInput->bindedListenSocket))
                {

                    perror("prepareServerInfo() - problem with close(): ");
                    prepareServerInfoRetVal = 1;

                    break;
                }

                perror("prepareServerInfo() - problem with bind(): ");

                continue;
            }

            break;
        }
    }

    freeaddrinfo(serverContainerInput->serverAddrInfo);

    if (!prepareServerInfoRetVal)
    {
        if (serverContainerInput->peerList == nullptr)
        {
            std::cerr << "prepareServerInfo() - server failed to bind() to address" << std::endl;

            prepareServerInfoRetVal = 1;
        }
    }

    return prepareServerInfoRetVal;
}

//change the clientDatagram struct from network to host order
inline void setClientDatagram(struct ClientDatagram *clientDatagramInput)
{

    clientDatagramInput->sequence_number = ntohl(clientDatagramInput->sequence_number);
    clientDatagramInput->payload_length = ntohs(clientDatagramInput->payload_length);
}

//set serverDatagram's contentes to network order based on contents of clientDatagram
inline void setServerDatagram(struct ServerDatagram *serverDatagramInput, struct ClientDatagram *clientDatagramInput)
{

    serverDatagramInput->sequence_number = htonl(clientDatagramInput->sequence_number);
    serverDatagramInput->datagram_length = htons(clientDatagramInput->payload_length + sizeof(struct ClientDatagram));
}

int mainServerLoop(struct serverContainer *serverContainerInput)
{

    int serverLoopRetVal = 0;

    int bytesRecevied = 0;
    int bytesSent = 0;

    int payloadDiffrence = 0;

    //stores address from where packet came from
    char addressString[INET6_ADDRSTRLEN];

    //possible payload that can be received and to compare against
    const char acknowledgePayload[] = "bdavis2";
    const char quitPayload[] = "quit111";

    //max possible size of clientDatagram struct
    const size_t udpStuctSize = sizeof(struct ClientDatagram) + sizeof(acknowledgePayload);

    struct ClientDatagram *clientDatagramInput;
    struct ServerDatagram serverUDPHolder;

    void *recvPayload = calloc(1, udpStuctSize);

    //point to the char payload section of client packet
    char *charSectionPayload = nullptr;

    if (recvPayload == nullptr)
    {

        perror("mainServerLoop() - problem with calloc(): ");

        serverLoopRetVal = 2;
    }

    if (!serverLoopRetVal)
    {//intialiaze struct used and set up clientDatagram struct

        memset(&serverUDPHolder, 0, sizeof(struct ServerDatagram));
        memset(&clientDatagramInput, 0, sizeof(struct ClientDatagram));

        clientDatagramInput = (struct ClientDatagram *)recvPayload;
        charSectionPayload = (char *)((char *)recvPayload + sizeof(struct ClientDatagram));

        std::cout << "starting server loop" << std::endl;
    }

    
    while (!serverLoopRetVal)
    {

        if ((bytesRecevied = recvfrom(serverContainerInput->bindedListenSocket, clientDatagramInput, udpStuctSize, 0, (struct sockaddr *)&serverContainerInput->connectedClientList, &serverContainerInput->addressLength)) == -1)
        {

            perror("mainServerLoop() - problem with recvfrom(): ");

            serverLoopRetVal = 1;

            break;
        }

        setClientDatagram(clientDatagramInput);

        if (serverDebugValue)
        {
                                                            //print address code
            std::cout << "Server log- got packet from: " << inet_ntop(serverContainerInput->connectedClientList.ss_family, getSocketAddress((struct sockaddr *)&serverContainerInput->connectedClientList), addressString, sizeof(addressString)) << std::endl;
            std::cout << "Server log- packet is " << bytesRecevied << " bytes long" << std::endl;
            std::cout << "Server log- sequence number: " << clientDatagramInput->sequence_number << " | payload length: " << clientDatagramInput->payload_length << std::endl;
            std::cout << "Server log- payload is: " << charSectionPayload << std::endl;
        }

        //check the recvied packet to make sure the payload is correct
        //if not check for quit packet
        //and it that fails exit program with error
        if ((payloadDiffrence = strncmp(acknowledgePayload, charSectionPayload, clientDatagramInput->payload_length)))
        {
            if ((payloadDiffrence = strncmp(quitPayload, charSectionPayload, clientDatagramInput->payload_length)) != 0)
            {

                std::cerr << "mainServerLoop() - payload did not match acknowledged one | diffrence: " << payloadDiffrence << std::endl;
                std::cerr << "bad payload was: " << recvPayload << std::endl;
                serverLoopRetVal = 1;

                break;
            }
            else
            {

                std::cout << "received quit packet " << std::endl;

                break;
            }
        }

        setServerDatagram(&serverUDPHolder, clientDatagramInput);

        if ((bytesSent = sendto(serverContainerInput->bindedListenSocket, &serverUDPHolder, sizeof(struct ServerDatagram), 0, (struct sockaddr *)&serverContainerInput->connectedClientList, sizeof(struct sockaddr))) == -1)
        {

            perror("mainServerLoop() - problem with sendto(): ");

            serverLoopRetVal = 1;

            break;
        }

        if (serverDebugValue)
        {
            std::cout << "Server log- sent packet to: " << inet_ntop(serverContainerInput->connectedClientList.ss_family, getSocketAddress((struct sockaddr *)&serverContainerInput->connectedClientList), addressString, sizeof(addressString)) << std::endl;
            std::cout << "Server log- packet is " << bytesSent << " bytes long" << std::endl;
        }

        memset(recvPayload, 0, udpStuctSize);
    }

    //free allocations
    if (serverLoopRetVal)
    {

        if (serverLoopRetVal != 2)
        {

            free(recvPayload);
        }
    }
    else
    {

        free(recvPayload);
    }

    return serverLoopRetVal;
}

int main(int argc, char *argv[])
{

    int routineRetVal = 0;

    struct argHolder programArgHolder;

    struct serverInfo serverInfoHolder;

    struct serverContainer serverContainerHolder;

    // where one is the ./programname arg if greater then that parse arguments
    if (argc > 1)
    {

        if ((routineRetVal = argHandler(argc, argv, &programArgHolder)))
        {

            std::cerr << "main() - Error in argHandler() " << std::endl;
        }
    }

    if (!routineRetVal)
    {

        routineRetVal = argProcessor(&programArgHolder, &serverInfoHolder);
    }

    if (!routineRetVal)
    {

        if (prepareServerInfo(&serverInfoHolder, &serverContainerHolder))
        {

            std::cerr << "main() - Error in prepareServerInfo() " << std::endl;

            routineRetVal = 1;
        }
    }

    if (!routineRetVal)
    {

        if ((routineRetVal = mainServerLoop(&serverContainerHolder)))
        {

            std::cerr << "main() - Error in mainServerLoop(): " << routineRetVal << std::endl;

            routineRetVal = 1;
        }

        if (close(serverContainerHolder.bindedListenSocket))
        {

            perror("main() - problem with close() bind socket: ");

            routineRetVal = 1;
        }
    }

    delete serverInfoHolder.bindingPort;

    return routineRetVal;
}