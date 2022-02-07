/*****************************************************************
* FILENAME :        interface.h 
*
*    Functions and datastructures for the interface
*    between users and the client program.
* 
*    DO NOT MODIFY THIS FILE.
*
* Version: 1.0
******************************************************************/
#ifndef INTERFACE_H_
#define INTERFACE_H_
#include <ctype.h>

// maximum size of data for the communication using TCP/IP
#define MAX_DATA 256

/*
 * This enum represents the result of a command.
 * Based on the response from the server,
 * you need to figure out the status.
 */
enum Status
{
    SUCCESS,
    FAILURE_ALREADY_EXISTS,
    FAILURE_NOT_EXISTS,
    FAILURE_INVALID,
    FAILURE_UNKNOWN
};

/* 
 * Reply structure is designed to be used for displaying the
 * result of the command that has been sent to the server.
 * For example, in the "process_command" function, you should
 * declare a variable of Reply structure and fill it based on 
 * the type of command and the result.
 * 
 * - CREATE and DELETE command:
 * Reply reply;
 * reply.status = one of values in Status enum;
 * 
 * - JOIN command:
 * Reply reply;
 * reply.status = one of values in Status enum;
 * reply.num_members = # of members
 * reply.port = port number;
 * 
 * - LIST command:
 * Reply reply;
 * reply.status = one of values in Status enum;
 * reply.list_room = list of rooms that have been create;
 *
 * This structure is not for communicating between server and client.
 * You need to design your own rules for the communication.
 */
struct Reply
{
    enum Status status;

    union {
        // Below structure is only for the "JOIN <chatroom name>" command
        struct {
            // # of members that have been joined the chatroom
            int num_member;
            // port number to join the chatroom
            int port;        
        };

        // list_room is only for the "LIST" command
        // contains the list of rooms that have been created
        char list_room[MAX_DATA];
    };
};

/* 
 * DO NOT MODIFY THIS FUNCTION
 * This function convert input string to uppercase.
 */
void touppercase(char *str, int n)
{
	int i;
    for (i = 0; str[i]; i++)
        str[i] = toupper((unsigned char)str[i]);
}

/* 
 * DO NOT MODIFY THIS FUNCTION
 * This function displays a title, commands that user can use.
 */
void display_title()
{
    printf("\n========= CHAT ROOM CLIENT =========\n");
    printf(" Command Lists and Format:\n");
    printf(" CREATE <name>\n");
    printf(" JOIN <name>\n");
    printf(" DELETE <name>\n");
    printf(" LIST\n");
    printf("=====================================\n");
}

/* 
 * DO NOT MODIFY THIS FUNCTION
 * This function prompts a user to enter a command.
 */
void get_command(char* comm, const int size)
{
    printf("Command> ");
    fgets(comm, size, stdin);
    comm[strlen(comm) - 1] = '\0';
}

/* 
 * DO NOT MODIFY THIS FUNCTION
 * This function prompts a user to enter a command.
 */
void get_message(char* message, const int size)
{
    fgets(message, size, stdin);
    message[strlen(message) - 1] = '\0';
}


/*
 * DO NOT MODIFY THIS FUNCTION.
 * You should call this function to display the message from 
 * other clients in a currently joined chatroom.
 */
void display_message(char* message)
{
    printf("> %s", message);
}

void display_reply(char* comm, const struct Reply reply)
{
	touppercase(comm, strlen(comm) - 1);
    switch (reply.status) {
        case SUCCESS:
            printf("Command completed successfully\n");
            if (strncmp(comm, "JOIN", 4) == 0) {
                printf("#Members: %d\n", reply.num_member);
				printf("#Port: %d\n", reply.port);
			} else if (strncmp(comm, "LIST", 4) == 0) {
				printf("List: %s\n", reply.list_room);
			}
            break;
        case FAILURE_ALREADY_EXISTS:
            printf("Input chatroom name already exists, command failed\n");
            break;
        case FAILURE_NOT_EXISTS:
            printf("Input chatroom name does not exists, command failed\n");
            break;
        case FAILURE_INVALID:
            printf("Command failed with invalid command\n");
            break;
        case FAILURE_UNKNOWN:
            printf("Command failed with unknown reason\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }
}

#endif // INTERFACE_H_
