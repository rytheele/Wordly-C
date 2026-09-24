#include "csapp.h"
#include <string.h>


//function that strips new lines (saftey measure for sanity of this project)
void removeNewLine(char *x){
    int stringlength = strlen(x); //get length of string
    if (x[stringlength-1] == '\n' && stringlength >0){ // if the last char in string is new line
        x[stringlength-1] = '\0'; // set it to null (\0)
    }
}

int main(int argc, char *argv[]) {
    int clientfd;  //file descriptor to communicate with the server
    char *host, *port; 
    char buffer[MAXLINE]; //MAXLINE = 8192 defined in csapp.h
    char instructions[5];

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	   exit(0);
    }

    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); //wrapper function that calls getadderinfo, socket and connect functions for client side

    //print menu for user
    printf("Hello! Welcome to Multiplayer Wordly. Guess The five letter word, the first player to guess correctly wins!\n");
    printf("Would you like to hear instructions? [y/n]: ");
        
    Fgets(instructions, 5, stdin);

    if(strcmp(instructions, "y\n") == 0){
        printf("To win, be the first player to guess the correct five letter word.\nYou will have unlimited guesses, not just six.\nYour input does not have to be a real word, but the answer will be, so keep that in mind.\nGood Luck!\n");
    }       

    
    char guess[7]; // declare  all values to make profile
    
     while (1) {
        printf("Enter your guess: ");
        Fgets(guess, sizeof(guess), stdin);

        if (strchr(guess, '\n') == NULL) { // I was having an issue where newline \n is left behind in the input buffer, which caused extra null guesses to be 
            int c;                         // So, if there's no newline in Guess, then Fgets didn't get the whole line.
            while ((c = getchar()) != '\n' && c != EOF) {} // throw away the extra characters so the next input prompt starts fresh
        }
         
        removeNewLine(guess); // remove the neline

        if (strlen(guess) != 5) { // Validate guess
            printf("That is an invalid guess, please reenter\n");
            continue;
        }

        write(clientfd, guess, strlen(guess)); // write to server

        bzero(buffer, MAXLINE);
        int n = read(clientfd, buffer, MAXLINE);
         
        if (n <= 0) { //tell client if server disconnects
            printf("Server disconnected.\n");
            break;
        }

        buffer[n] = '\0';
        printf("Message from server:\n%s\n", buffer); //message from server to show client's buffer

        if (strstr(buffer, "winner ended the game") != NULL || strstr(buffer, "Please reconnect") != NULL) { // break if the winner said no to another round
            break;
        }


         //The server asks whether the winner wants to play again, read a y/n answer, send it to the server, read the server’s follow-up message, and exit if the game is done.

        if (strstr(buffer, "Play again?") != NULL) {
            char answer[8];
        
            Fgets(answer, sizeof(answer), stdin);
            removeNewLine(answer);
        
            write(clientfd, answer, strlen(answer));
        
            bzero(buffer, MAXLINE);
            int n = read(clientfd, buffer, MAXLINE);
        
            if (n <= 0) {
                printf("Server disconnected.\n");
                break;
            }
        
            buffer[n] = '\0';
            printf("%s\n", buffer);
        
            if (strstr(buffer, "Thanks for playing") != NULL) {
                break;
            }
         
        }
    
     }

    Close(clientfd); //close!
    return 0;
}

