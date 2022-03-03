#ifdef _WIN32
	#include <windows.h>
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
	#include <sys/mman.h>
	#include <fcntl.h> /* For O_* constants */
#endif // _WIN32

#include "external/SharedMemory.h"

/* 
 * usage: wrapper <minetest_server_host> <nick>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <wchar.h>
#include <thread>
#include <iostream>
#include <csignal>
#include <sstream>
#include <regex>

#define BUFSIZE 1024
char buf[BUFSIZE];
int set_player_time = 0;

struct LinkedMem {

#ifdef _WIN32
	UINT32	uiVersion;
	DWORD	uiTick;
#else
	uint32_t uiVersion;
	uint32_t uiTick;
#endif
	float	fAvatarPosition[3];
	float	fAvatarFront[3];
	float	fAvatarTop[3];
	wchar_t	name[256];
	float	fCameraPosition[3];
	float	fCameraFront[3];
	float	fCameraTop[3];
	wchar_t	identity[256];
#ifdef _WIN32
	UINT32	context_len;
#else
	uint32_t context_len;
#endif
	unsigned char context[256];
	wchar_t description[2048];
};

SharedMemory sharedMem;
LinkedMem *lm = NULL;

void initMumble() {
#ifdef _WIN32
	HANDLE hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (hMapObject == NULL)
		return;

	lm = (LinkedMem *) MapViewOfFile(hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
	if (lm == NULL) {
		CloseHandle(hMapObject);
		hMapObject = NULL;
		return;
	}
#else
	char memname[256];
	snprintf(memname, 256, "/MumbleLink.%d", getuid());

	int shmfd = shm_open(memname, O_RDWR, S_IRUSR | S_IWUSR);

	if (shmfd < 0) {
		return;
	}

	lm = (LinkedMem *)(mmap(NULL, sizeof(struct LinkedMem), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd,0));

	if (lm == (void *)(-1)) {
		lm = NULL;
		return;
	}
#endif
}

void setPositionalPointers(std::string line) {
    char pattern[255] = "^([cp])\\s([pl])\\s\\[([-]?[0-9]*[.]?[0-9]*)\\s([-]?[0-9]*[.]?[0-9]*)\\s([-]?[0-9]*[.]?[0-9]*)\0";
    // regex expression for pattern to be searched 
    std::regex regexp(pattern); 
    // flag type for determining the matching behavior (in this case on string objects)
    std::smatch m; 
    // regex_search that searches pattern regexp in the string mystr  
    regex_search(line, m, regexp); 
  
    if (m[0] != "") {
        //std::cout << "--> " << m[0] << std::endl;
        if(m[1] == "p" && m[2] == "p") {
	        // Position of the avatar (here standing slightly off the origin)
	        lm->fAvatarPosition[0] = std::stof(m[3]);
	        lm->fAvatarPosition[1] = std::stof(m[4]);
	        lm->fAvatarPosition[2] = std::stof(m[5]);
        }
        if(m[1] == "p" && m[2] == "l") {
	        // Unit vector pointing out of the avatar's eyes aka "At"-vector.
	        lm->fAvatarFront[0] = std::stof(m[3]);
	        lm->fAvatarFront[1] = std::stof(m[4]);
	        lm->fAvatarFront[2] = std::stof(m[5]);
        }
        if(m[1] == "c" && m[2] == "p") {
	        // Same as avatar but for the camera.
	        lm->fCameraPosition[0] = std::stof(m[3]);
	        lm->fCameraPosition[1] = std::stof(m[4]);
	        lm->fCameraPosition[2] = std::stof(m[5]);
        }
        if(m[1] == "c" && m[2] == "l") {
	        lm->fCameraFront[0] = std::stof(m[3]);
	        lm->fCameraFront[1] = std::stof(m[4]);
	        lm->fCameraFront[2] = std::stof(m[5]);
        }
    }
    /*
    // Get the Position item that we need to set something on.
        'p' => &mut player,
        'c' => &mut camera,
    // Figure out which component to set.
        'p' => target.position = vec,
        'l' => target.front = vec,
    test: p p [-450.72900390625 8 -450.4880065918]
    test: p l [-0.80917203426361 -0.36877354979515 0.45743483304977]
    test: c p [-450.72900390625 8 -450.4880065918]
    test: c l [-0.80917203426361 -0.36877354979515 0.45743483304977]
    */
}

void setIdentifier(std::string line) {
    char pattern[255] = "(mumble\\sid)\\s(\\w+)\0";
    // regex expression for pattern to be searched 
    std::regex regexp(pattern); 
    // flag type for determining the matching behavior (in this case on string objects)
    std::smatch m; 
    // regex_search that searches pattern regexp in the string mystr  
    regex_search(line, m, regexp);
    if (m[0] != "") {
	    // Identifier which uniquely identifies a certain player in a context (e.g. the ingame name).
        if(m[1] == "mumble id") {
            std::cout << "--> " << m.str(0) << std::endl;
            std::string str = m.str(2);
            std::wstring widestr = std::wstring(str.begin(), str.end());
            int cSize = 256;
	        wcsncpy(lm->identity, widestr.c_str(), cSize);
        }
    }
}

void setContext(std::string line) {
    char pattern[255] = "(mumble\\scontext)\\s(([a-zA-Z0-9][a-zA-Z0-9-]+[a-zA-Z0-9]\\.[^\\s]{2,}|[a-zA-Z0-9][a-zA-Z0-9-]+[a-zA-Z0-9]\\.[^\\s]{2,}|[a-zA-Z0-9]+\\.[^\\s]{2,}|[a-zA-Z0-9]+\\.[^\\s]{2,}))\0";
    // regex expression for pattern to be searched 
    std::regex regexp(pattern); 
    // flag type for determining the matching behavior (in this case on string objects)
    std::smatch m; 
    // regex_search that searches pattern regexp in the string mystr  
    regex_search(line, m, regexp);
    if (m[0] != "") {
	    // Identifier which uniquely identifies a certain player in a context (e.g. the ingame name).
        if(m[1] == "mumble context") {
            std::cout << "--> " << m.str(0) << std::endl;
            std::string str = m.str(2);
	        memcpy(lm->context, static_cast<void*>(&str), 16);
	        lm->context_len = 16;
        }
    }
}

void updateMumble() {
	if (! lm)
		return;

	if(lm->uiVersion != 2) {
		wcsncpy(lm->name, L"MineTestLink", 256);
		wcsncpy(lm->description, L"MineTestLink is a wrapper for the Link plugin.", 2048);
		lm->uiVersion = 2;
	}

	lm->uiTick++;

    std::string line;
    std::string data(buf);
    std::stringstream data_stream(data);

    while(std::getline(data_stream, line)) 
    {        
        //std::cout << "test: " << line << std::endl;
        setPositionalPointers(line);
        setIdentifier(line);
        setContext(line);

    }

	// Left handed coordinate system.
	// X positive towards "right".
	// Y positive towards "up".
	// Z positive towards "front".
	//
	// 1 unit = 1 meter

	// Unit vector pointing out of the top of the avatar's head aka "Up"-vector (here Top points straight up).
	lm->fAvatarTop[0] = 0.0f;
	lm->fAvatarTop[1] = 1.0f;
	lm->fAvatarTop[2] = 0.0f;

	lm->fCameraTop[0] = 0.0f;
	lm->fCameraTop[1] = 1.0f;
	lm->fCameraTop[2] = 0.0f;

	// Identifier which uniquely identifies a certain player in a context (e.g. the ingame name).
	//wcsncpy(lm->identity, L"Unique ID", 256);
	// Context should be equal for players which should be able to hear each other positional and
	// differ for those who shouldn't (e.g. it could contain the server+port and team)
	//memcpy(lm->context, "ContextBlob\x00\x01\x02\x03\x04", 16);
	//lm->context_len = 16;
}

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void signalHandler(int signum) {
	std::cout << "Interrupt signal (" << signum << ") received - shutting down..." << std::endl;

	lm = nullptr;
	sharedMem.close();

	std::exit(signum);
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char *nick;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <nick>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    nick = argv[2];
    
    portno = 44000;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    //bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    memcpy ( (char *)&serveraddr.sin_addr.s_addr, (char *)server->h_addr, server->h_length );
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    memset(buf, 0, BUFSIZE);
    //printf("Please enter msg: ");
    //fgets(buf, BUFSIZE, stdin);

    char user[255];
    memset(user, 0, sizeof(user));
    strncpy(user, nick, sizeof(user)-1); /* OK ... but `dst` needs to be NUL terminated */
    //printf ("User: %s \n", nick);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, user, strlen(nick), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) {
        error("no connection");
        return 1;
    }

    //link integration
	signal(SIGINT, signalHandler);
	initMumble();

	if (!lm) {
		std::cerr << "Failed to create shared memory region (" << sharedMem.lastError() << ")" << std::endl;
		return 1;
	}

	printf("Shared memory created successfully - Now starting update loop\n");
    //end link integration


    /* print the server's reply */
    int wait_miliseconds = 100;
    while (1) {
        set_player_time += wait_miliseconds;
        if (set_player_time >= 1000) {
            set_player_time = 0;
            n = sendto(sockfd, user, strlen(nick), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) {
                error("no connection");
                return 1;
            }            
        }
        //printf("Tick\n");
		updateMumble();

        #ifdef _WIN32
        n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &serveraddr, (int*) &serverlen);
        Sleep(wait_miliseconds);
        #else
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_miliseconds));
        n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &serveraddr, (socklen_t*) &serverlen);
        #endif
        if (n < 0) 
            error("error on receiving");
        //printf("%s", buf);
    }
    return 0;
}



