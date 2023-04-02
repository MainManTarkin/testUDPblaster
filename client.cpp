#include <iostream>
#include <iomanip>
#include <string>
#include <set>
#include <cstdlib>

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <memory.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>

#include "defaults.hpp"
#include "structure.hpp"

bool clientDebugValue = false;

const char clientHelpText[] =
    "|---------------------------------------------------|\n"
    "|====================== Help =======================|\n"
    "|    -h - prints out program arg contents           |\n"
    "|    -p - defines port number other then defualt    |\n"
    "|         39390                                     |\n"
    "|    -s - redefine default address c:localhost      |\n"
    "|    -y - override default datagrams to send        |\n"
    "|    -n - add a delay from sending                  |\n"
    "|    -d - activate debugging info                   |\n"
    "|    -t - how many times a recvfrom should try      |\n"
    "|         to recv defualt is 1                      |\n"
    "|---------------------------------------------------|\n";

struct argHolder
{

    argHolder() = default;

    // boolean values that are used by arClienthandler to know what to load into struct clientInfo
    bool helpArg = false;
    bool portRedefineArg = false;
    bool addressRedefine = false;
    bool numberDGOverride = false;
    bool isThereDelay = false;
    bool recvLoopOveride = false;

    char *portName = nullptr;
    char *addressName = nullptr;

    int numberOfDG = 0;
    int delayTime = 0;
    int recvfromTryTimes = 0;
};

struct clientInfo
{

    clientInfo() = default;

    int dataGramsToSend = NUMBER_OF_DATAGRAMS;

    int nanoDelay = 0;

    int recvLoopTries = 1;

    int clientSocket = -1;

    // address and port to talk to
    char *addressToSend = nullptr;
    char *portUsed = nullptr;

    // infomation for communicating to server
    struct addrinfo basedInfo;
    struct addrinfo *serverAddrInfo = nullptr;
    struct addrinfo *addList = nullptr;

    // storage for address regardless of protocal
    struct sockaddr_storage connectedClientList;

    // socket address length
    socklen_t addressLength = sizeof(struct sockaddr_storage);
};

// inline functions are used since they are only ever called once

inline int programCleanUp(struct clientInfo *clientInfoInput)
{

    int cleanUpRetVal = 0;

    delete clientInfoInput->addressToSend;
    delete clientInfoInput->portUsed;

    freeaddrinfo(clientInfoInput->serverAddrInfo);

    if (clientInfoInput->clientSocket != -1)
    {

        if ((cleanUpRetVal = close(clientInfoInput->clientSocket)))
        {

            perror("programCleanUp() - problem with close() socket: ");
        }
    }

    return cleanUpRetVal;
}

inline int argHandler(int argc, char *argv[], struct argHolder *argHolderInput)
{

    int argHandlerRetVal = 0;
    int optRetVal = 0;

    while ((optRetVal = getopt(argc, argv, "hdp:s:y:n:t:")) != -1 && argHandlerRetVal == 0 && !argHandlerRetVal)
    {

        switch (optRetVal)
        {
        case 'h':

            argHolderInput->helpArg = true;

            break;
        case 't':

            argHolderInput->recvLoopOveride = true;

            argHolderInput->recvfromTryTimes = atoi(optarg);

            break;
        case 'p':
            argHolderInput->portRedefineArg = true;
            argHolderInput->portName = new char(strlen(optarg));

            if (argHolderInput->portName == nullptr)
            {

                std::cerr << "argHandler() - error acquiring memory with new for portName" << std::endl;
                argHandlerRetVal = 1;
                break;
            }

            strcpy(argHolderInput->portName, optarg);

            break;
        case 's':
            argHolderInput->addressRedefine = true;
            argHolderInput->addressName = new char(strlen(optarg));

            if (argHolderInput->addressName == nullptr)
            {

                std::cerr << "argHandler() - error acquiring memory with new for addressName" << std::endl;
                argHandlerRetVal = 1;
                break;
            }

            strcpy(argHolderInput->addressName, optarg);
            break;
        case 'd':

            clientDebugValue = true;
            break;
        case 'y':
            argHolderInput->numberDGOverride = true;
            argHolderInput->numberOfDG = atoi(optarg);

            break;
        case 'n':
            argHolderInput->isThereDelay = true;
            argHolderInput->delayTime = atoi(optarg);

            break;
        default:
            // if any defualt case occurs it is an error so set return value to one end loop and return
            argHandlerRetVal = 1;

            std::cerr << "argHandler() - ran into defualt case: " << std::endl;

            break;
        }
    }

    if (argHandlerRetVal)
    {

        // clean up possible allocations

        if (argHolderInput->portName != nullptr)
        {

            delete argHolderInput->portName;
        }

        if (argHolderInput->addressName != nullptr)
        {

            delete argHolderInput->addressName;
        }
    }

    return argHandlerRetVal;
}

inline int argClientHandler(struct argHolder *argHolderInput, struct clientInfo *clientInfoInput)
{

    int argClientRetVal = 0;

    // if the -s option is never given use the defualt address and save it size for strcpy
    const char defualtAddress[] = SERVER_IP;
    size_t addressLength = sizeof(defualtAddress);

    // look at the list of booleans in argHolderInput
    // if any of them are true then add that to the client struct

    if (argHolderInput->helpArg)
    {

        argClientRetVal = 1;

        std::cout << clientHelpText << std::endl;
    }

    if (!argClientRetVal)
    {
        if (argHolderInput->addressRedefine)
        {

            clientInfoInput->addressToSend = argHolderInput->addressName;
        }
        else
        {

            clientInfoInput->addressToSend = new char(addressLength);

            if (clientInfoInput->addressToSend == nullptr)
            {

                argClientRetVal = 1;
            }
            else
            {

                strncpy(clientInfoInput->addressToSend, defualtAddress, addressLength);
            }
        }
    }

    if (!argClientRetVal)
    {

        if (argHolderInput->portRedefineArg)
        {

            clientInfoInput->portUsed = argHolderInput->portName;
        }
        else
        {
            // where 6 denotes the length of the port in string format
            clientInfoInput->portUsed = new char(6);

            sprintf(clientInfoInput->portUsed, "%d", PORT_NUMBER);
        }
    }

    if (!argClientRetVal)
    {

        if (argHolderInput->isThereDelay)
        {

            clientInfoInput->nanoDelay = argHolderInput->delayTime;
        }

        if (argHolderInput->numberDGOverride)
        {

            clientInfoInput->dataGramsToSend = argHolderInput->numberOfDG;
        }

        if (argHolderInput->recvLoopOveride)
        {

            clientInfoInput->recvLoopTries = argHolderInput->recvfromTryTimes;
        }
    }

    return argClientRetVal;
}

void *getSocketAddress(struct sockaddr *socketAddInput)
{
    if (socketAddInput->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)socketAddInput)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)socketAddInput)->sin6_addr);
}

inline int prepareClient(struct clientInfo *clientInfoInput)
{

    int prepareClientRetVal = 0;
    int errCode = 0;

    memset(&clientInfoInput->basedInfo, 0, sizeof(struct addrinfo));

    // set ip use and tlp to use
    clientInfoInput->basedInfo.ai_family = AF_UNSPEC;
    clientInfoInput->basedInfo.ai_socktype = SOCK_DGRAM;

    if ((errCode = getaddrinfo(clientInfoInput->addressToSend, clientInfoInput->portUsed, &clientInfoInput->basedInfo, &clientInfoInput->serverAddrInfo)) != 0)
    {

        std::cerr << "prepareClient() - problem with getaddrinfo(): " << gai_strerror(errCode) << std::endl;

        freeaddrinfo(clientInfoInput->serverAddrInfo);

        prepareClientRetVal = 1;
    }

    // looping to find adderinfo for socket creation
    if (!prepareClientRetVal)
    {

        for (clientInfoInput->addList = clientInfoInput->serverAddrInfo; clientInfoInput->addList != nullptr; clientInfoInput->addList = clientInfoInput->addList->ai_next)
        {

            if ((clientInfoInput->clientSocket = socket(clientInfoInput->addList->ai_family, clientInfoInput->addList->ai_socktype, clientInfoInput->addList->ai_protocol)) == -1)
            {

                perror("prepareClient() - problem with socket() creation: ");
                continue;
            }

            break;
        }

        if (clientInfoInput->addList == nullptr)
        {
            std::cerr << "prepareClient() - failed to create socket()" << std::endl;
            prepareClientRetVal = 1;
        }
    }

    // set socket to non-blocking
    if (!prepareClientRetVal)
    {

        if (fcntl(clientInfoInput->clientSocket, F_SETFL, O_NONBLOCK) == -1)
        {

            perror("prepareClient() - problem setting the socket to non-blocking: ");

            prepareClientRetVal = 1;
        }
    }

    return prepareClientRetVal;
}

// convert contents of struct input to host order from network order
inline void covertUDPtoNTOH(struct ServerDatagram *clientUDPInput)
{

    clientUDPInput->sequence_number = ntohl(clientUDPInput->sequence_number);

    clientUDPInput->datagram_length = ntohs(clientUDPInput->datagram_length);
}

//function for running recv mutiple times (if stated in clientInfo) to catch bursty packets
inline int recvFromCatch(struct clientInfo *clientInfoInput, struct ServerDatagram *ServerDatagramInput)
{

    int bytesRecv = 0;

    for (int i = 0; i < clientInfoInput->recvLoopTries; i++)
    {

        if ((bytesRecv = recvfrom(clientInfoInput->clientSocket, ServerDatagramInput, sizeof(struct ServerDatagram), 0, (struct sockaddr *)&clientInfoInput->connectedClientList, &clientInfoInput->addressLength)) == -1)
        {
            if (errno != EAGAIN || errno != EWOULDBLOCK)
            {
                perror("recvFromCatch() - error with recvfrom(): ");

                bytesRecv = -1;

                break;
            }
        }

        //if packet was recvied then end loop and function
        //else reset bytesRecv and keep looping untill end
        if(bytesRecv > 0){

            break;

        }else{

            bytesRecv = 0;

        }
    }

    return bytesRecv;
}

inline int mainClientLoop(struct clientInfo *clientInfoInput)
{

    int clientLoopRetVal = 0;

    int bytesSent = 0;
    int bytesRecv = 0;

    // addres conversion variables
    char addressString[INET6_ADDRSTRLEN];

    // payload variables are constant and will stay the same in the client UDP struct

    const char acknowledgePayload[] = "bdavis2";
    const char quitPayload[] = "quit111";

    // full length of client UDP struct with added payload
    const size_t udpStuctSize = sizeof(struct ClientDatagram) + sizeof(acknowledgePayload);

    // Points to the payload section of the struct
    char *charSectionPayload = nullptr;

    // recevied structer setup
    struct ServerDatagram clientUDPRecvied;
    memset(&clientUDPRecvied, 0, sizeof(struct ServerDatagram));

    std::set<int> udpSet;

    // clientUDP shall have it payload casted as struct to offset of payload
    struct ClientDatagram *clientUDP = (struct ClientDatagram *)calloc(1, udpStuctSize);

    if (clientUDP == nullptr)
    {

        perror("mainClientLoop() - problem with allocateing memory for clientUDP: ");

        clientLoopRetVal = 2;
    }

    // is used to setup clientDatagram and setup the payload
    if (!clientLoopRetVal)
    {
        clientUDP->sequence_number = 0;
        clientUDP->payload_length = htons(8);

        charSectionPayload = (char *)((char *)clientUDP + sizeof(struct ClientDatagram));
        strncpy(charSectionPayload, acknowledgePayload, sizeof(acknowledgePayload));
    }

    // the primary UDP blaster loop set to only send the amount specified
    if (!clientLoopRetVal)
    {

        for (int i = 0; i < clientInfoInput->dataGramsToSend; i++)
        {

            // use i for current packet-seq being sent and add it into set

            udpSet.insert(i);
            // prepare data for network byte order

            clientUDP->sequence_number = htonl(i);

            // send and delay (if told too)
            if ((bytesSent = sendto(clientInfoInput->clientSocket, (void *)clientUDP, udpStuctSize, 0, clientInfoInput->addList->ai_addr, clientInfoInput->addList->ai_addrlen)) == -1)
            {

                if (errno != EAGAIN || errno != EWOULDBLOCK)
                {
                    perror("mainClientLoop() - error with sendto(): ");

                    clientLoopRetVal = 1;

                    break;
                }
            }

            if (bytesSent > 0)
            {

                if (clientDebugValue)
                {

                    std::cout << "send " << bytesSent << " bytes to: " << clientInfoInput->addressToSend << std::endl;
                }
            }
            else
            {
                // if no bytes where sent and this was not related to error specified as EAGAIN EWOULDBLOCK
                // then decrease loop value (thus doing it agian as most likely case send buffer is too full)
                // but keep going with the loop to see if we can recv anything
                i--;
            }

            // delay block
            if (clientInfoInput->nanoDelay > 0)
            {

                if (usleep(clientInfoInput->nanoDelay))
                {

                    perror("mainClientLoop() - problem with usleep(): ");

                    clientLoopRetVal = 1;

                    break;
                }
            }

            if((bytesRecv = recvFromCatch(clientInfoInput, &clientUDPRecvied)) == -1){

                std::cerr << "mainClientLoop() - problem in recvFromCatch()" << std::endl;

                clientLoopRetVal = 1;

                break;

            }


            if (bytesRecv > 0)
            {
                covertUDPtoNTOH(&clientUDPRecvied);

                // a check to see if packet recieved has the antciapted length that should always be equal to udpStuctSize
                if (clientUDPRecvied.datagram_length != udpStuctSize)
                {

                    std::cerr << "packet-" << clientUDPRecvied.sequence_number << " reported invaild length: " << clientUDPRecvied.datagram_length << std::endl;

                    clientLoopRetVal = 1;

                    break;
                }

                if (clientDebugValue)
                {

                    std::cout << "received " << bytesRecv << " bytes from: " << inet_ntop(clientInfoInput->connectedClientList.ss_family, getSocketAddress((struct sockaddr *)&clientInfoInput->connectedClientList), addressString, sizeof(addressString)) << std::endl;
                    std::cout << "servers received packet seq-number: " << clientUDPRecvied.sequence_number << " and is " << clientUDPRecvied.datagram_length << " bytes long" << std::endl;
                }

                // bookeeping
                udpSet.erase(clientUDPRecvied.sequence_number);
            }
        }
    }

    // send quit packet to server so its program ends but only functions in debug mode
    if (clientDebugValue)
    {
        bytesSent = 0;
        clientUDP->sequence_number = htonl(0);
        strncpy(charSectionPayload, quitPayload, sizeof(quitPayload));
        while (bytesSent <= 0)
        {
            if ((bytesSent = sendto(clientInfoInput->clientSocket, (void *)clientUDP, udpStuctSize, 0, clientInfoInput->addList->ai_addr, clientInfoInput->addList->ai_addrlen)) == -1)
            {

                if (errno != EAGAIN || errno != EWOULDBLOCK)
                {
                    perror("mainClientLoop() - error with sendto(): ");

                    clientLoopRetVal = 1;

                    break;
                }
            }
        }
    }

    if (!clientLoopRetVal)
    {

        std::cout << "every packet sent" << std::endl;
        std::cout << "packets dropped: " << udpSet.size() << std::endl;
    }

    if (clientLoopRetVal < 2)
    {

        free(clientUDP);
    }

    return clientLoopRetVal;
}

int main(int argc, char *argv[])
{

    int mainRetVal = 0;

    struct argHolder programArgHolder;

    struct clientInfo clientInfoHolder;

    // only bother to run argHandler if there is more then ./(program name)
    if (argc > 1)
    {

        if (argHandler(argc, argv, &programArgHolder))
        {

            std::cerr << "main() - problem with argHandler()" << std::endl;

            mainRetVal = 1;
        }
    }

    if (!mainRetVal)
    {

        mainRetVal = argClientHandler(&programArgHolder, &clientInfoHolder);
    }

    if (!mainRetVal)
    {

        if ((mainRetVal = prepareClient(&clientInfoHolder)))
        {

            std::cerr << "main() - problem with prepareClient()" << std::endl;
        }
    }

    if (!mainRetVal)
    {

        if ((mainRetVal = mainClientLoop(&clientInfoHolder)))
        {

            std::cerr << "main() - problem with mainClientLoop()" << std::endl;
        }
    }

    if ((mainRetVal = programCleanUp(&clientInfoHolder)))
    {

        std::cerr << "main() - problem with programCleanUp()" << std::endl;
    }

    return mainRetVal;
}