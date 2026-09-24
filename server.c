#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

// added from last project as a saftey measure.

sem_t mutex;    // controls access to reader count
sem_t db;       // controls access to database
sem_t q;        // establish the queue 


// define all colors
#define GREEN   "\e[0;32m"
#define YELLOW "\e[0;33m"
#define RED "\e[0;31m"
#define RESET "\e[0m"


#define MAX_CLIENTS 100

int clients[MAX_CLIENTS];

sem_t clientMutex;

pthread_mutex_t replayMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t replayCond = PTHREAD_COND_INITIALIZER;

int waitingPlayers = 0;
int activePlayers = 0;
int roundOver = 0;
int clientCount = 0;
char key[MAXLINE];
int rc = 0;




//function that strips new lines (saftey measure for sanity of this project)
void removeNewLine(char *x){
    int stringlength = strlen(x); //get length of string
    if (stringlength > 0 && x[stringlength-1] == '\n'){ // if the last char in string is new line
        x[stringlength-1] = '\0'; // set it to null (\0)
    }
    
    //remove leading whitespace, too...
    if(x[0] == '\n'){
        x[0] = '\0';
    }    
}

//function to make write() calls simpler (no need to manually count chars and can handle dynamic input)
void writeString(int fd, const char *s) {
    write(fd, s, strlen(s));
}  
 
            
// chose a word from file to act as key.
char* chooseWord(){
    int num = rand() % 150;
    
    FILE *fp = fopen("words.txt", "r");
    static char line[MAXLINE];
    
    for(int i = 0; i <= num; i++) {
        fgets(line, MAXLINE, fp);
    }
    
    fclose(fp);  
    removeNewLine(line);
    printf("Chosen word: %s\n", line);
    return line;
}


// Lots of the main logic for checking happens here

char* checkGuess(char *guess, char *key){
    static char coloredGuess[MAXLINE];
    char tempKey[6]; // key copy that I can destroy to allow me to check for duplicate letters without returning incorrect results

    strcpy(tempKey, key);
    coloredGuess[0] = '\0';

    int status[5]; // tracks matched positions.  0 = red, 1 = green, 2 = yellow,


    // first pass to see what letters will return green and red.

    for(int i = 0; i < 5; i++){
        if(guess[i] == tempKey[i]){
            status[i] = 1; // gets marked as 1 for building string later
            tempKey[i] = '#';
        } else{
            status[i] = 0; // not exactly promised to be red yet, but wil be updated in next pass
        }
    }


    // since im comparing using the key copy, I can hash out letters that have been "used up" already. 
    // For example, lets say the word is "AGREE" and the user gussed "EEEEE", the output using very simple matching logic would print three yellow E's and two green E's.
    // In the original wordle game, this would imply that there are three E's in the wrong spot, which is incorrect, so the logic needs to know when a letter has been found in a word.
    
    // second pass to check for yellow and confirms which letters will remain red.
    for(int i =0; i < 5; i++){
        if(status[i] == 0){
            for(int j = 0; j < 5; j++){
                if(guess[i] == tempKey[j]){ //
                    status[i] = 2;
                    tempKey[j] = '#';
                    break;
                }
            }
            
        }
    }


    // build string from the status data.
    for(int i = 0; i < 5; i++){
        if(status[i] == 1){
            strcat(coloredGuess, GREEN);
        } else if(status[i] == 2){
            strcat(coloredGuess, YELLOW);
        } else{
            strcat(coloredGuess, RED);
        }
    
        int len = strlen(coloredGuess);
        coloredGuess[len] = guess[i];
        coloredGuess[len + 1] = '\0';

        strcat(coloredGuess, RESET);
    }

    removeNewLine(coloredGuess);
    return coloredGuess;
}



// converts clients guess to uppercase for string comparisions
char* toUpper(char* Guess){
    for (int i = 0; Guess[i] != '\0'; i++) { //while guess is not null
        Guess[i] = toupper(Guess[i]); // use toupper func on every char & assign to Guess
    }
    return Guess; // return
    
}

void serverFunction(int connfd){
    char buffer[MAXLINE];

    while(1){

        // this beginning handles input. the loop waits for client input, whether that be a guess or to keep going
        bzero(buffer, MAXLINE);

        int n = read(connfd, buffer, MAXLINE);

        if(n <= 0){
            break;
        }

        buffer[n] = '\0';
        printf("SERVER RECEIVED: '%s'\n", buffer);

        removeNewLine(buffer);
        toUpper(buffer);

        int won = 0; // if won variable

        sem_wait(&db);
        
        // if the round is not already over and the client's guess is correct, mark client as won and roundOver to true
        if (!roundOver && strcmp(buffer, key) == 0) {
            roundOver = 1;
            won = 1;
        }

        sem_post(&db);


        //
        if (won) {
            char msg[MAXLINE];

            msg[0] = '\0';

            
            //strcat logic, very similar to last project
            strcat(msg, GREEN);
            strcat(msg, buffer);
            strcat(msg, RESET);
            strcat(msg, "\n");
        
            strcat(msg, GREEN);
            strcat(msg, "Correct! You win!");
            strcat(msg, RESET);
            strcat(msg, "\nPlay again? [y/n]: "); // asking for play again logic
        
            writeString(connfd, msg); // call to function

  

            bzero(buffer, MAXLINE); //zero out buffer
            n = read(connfd, buffer, MAXLINE);

            if (n <= 0) {
                break;
            }

            buffer[n] = '\0';
            removeNewLine(buffer); 
            toUpper(buffer);

            if (strcmp(buffer, "N") == 0) {
                writeString(connfd, "Thanks for playing!\n");

                sem_wait(&clientMutex);

                for (int i = 0; i < clientCount; i++) {
                    if (clients[i] != connfd) {
                        writeString(clients[i],
                            "The winner ended the game. Please reconnect to play again.\n");

                        shutdown(clients[i], SHUT_RDWR); // !!! This solved a bug that I was having in an earlier verson. I was using Close(), which would close the same socket twice, which caused the server to crash
                                                        // shutdown effectively tells the socket to shutdown both send and receive operations (using SHUT_RDWR). Its apart of <sys/socket.h>. It kills the socket without
                                                        // causing issues within the logic flow
                    }
                }

                sem_post(&clientMutex);

                sem_wait(&db);
                roundOver = 0;
                strcpy(key, chooseWord());
                sem_post(&db);

                break;
            }

            sem_wait(&db);
            strcpy(key, chooseWord()); 
            roundOver = 0;
            sem_post(&db);

            writeString(connfd, "New round started!\n");

            continue;
        }

        sem_wait(&db);

        if (roundOver) { //if the round is over, but a client has not restarted or ended a round, tell other clients.
            sem_post(&db);

            writeString(connfd, "This round is over. Waiting for the next round...\n");

            continue;
        }

        sem_post(&db);

        sem_wait(&mutex);

        rc++;

        if(rc == 1){
            sem_wait(&db);
        }

        sem_post(&mutex);

        char resultMsg[MAXLINE];

        strcpy(resultMsg, checkGuess(buffer, key));
        strcat(resultMsg, "\n");

        printf("SERVER SENDING: '%s'\n", resultMsg); // sanity in server output

        writeString(connfd, resultMsg);

        sem_wait(&mutex); //all of this is exit proceedure

        rc--;

        if(rc == 0){
            sem_post(&db);
        }

        sem_post(&mutex);
    }
}


void *thread(void *vargp){
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());

    free(vargp);

    //function to interact with the client
    serverFunction(connfd);

    sem_wait(&clientMutex);

    for (int i = 0; i < clientCount; i++) { // for each client, manage the current client count
        if (clients[i] == connfd) {
            clients[i] = clients[clientCount - 1];
            clientCount--;
            activePlayers--;
        break;
        }
    }

    sem_post(&clientMutex);
   
    Close(connfd); // final close of the sockets

    return NULL;
}



int main(int argc, char *argv[]) {
    // declare values
    sem_init(&mutex,0,1);
    sem_init(&db,0,1);
    sem_init (&q,0,1);

    srand(time(NULL));
    strcpy(key, chooseWord());
    
    int listenfd;
    int connfd; //file descriptor to communicate with the client
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */

    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    pthread_t tid;

    
    listenfd = Open_listenfd(argv[1]); //wrapper function that calls getadderinfo, socket, bind, and listen functions in the server side

    //Server runs in the infinite loop.
    //To stop the server process, it needs to be killed using the Ctrl+C key.


    sem_init(&clientMutex, 0, 1);
    
    while (1) {
    	clientlen = sizeof(struct sockaddr_storage);

        int *connfd_ptr = malloc(sizeof(int));

        *connfd_ptr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        sem_wait(&clientMutex); // Lock the client list, add this new client socket if there is room, update the count, then unlock the client list.

        if (clientCount < MAX_CLIENTS) {
            clients[clientCount++] = *connfd_ptr;
            activePlayers++;
        }

        sem_post(&clientMutex);

        
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE,client_port, MAXLINE, 0);

        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        pthread_create(&tid, NULL, thread, connfd_ptr);
    }

    
    //destroy for sanity
    sem_destroy(&mutex);
    sem_destroy(&db);
    sem_destroy (&q);
    exit(0);

}