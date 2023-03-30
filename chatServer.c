//By: Yahya Saad ID: 322944869
//Ex4: chatServer.c
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "chatServer.h"
//-----some macros -----
#define TURN_ON 1
#define TURN_OFF 0
#define MAX_TO_LISTEN 5
#define MAX_PORT_VAL 65536  // 2^16
//-----some private functions -----
void printUsage();          // usage error
void systemError(char *);   // return error msg
//main code
static int end_server = TURN_OFF;
void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    end_server = TURN_ON;
}
int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    // checks if received wrong parameter.
    if (argc != 2 || (port < 1 || port > MAX_PORT_VAL)) {
        printUsage();
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, intHandler);
    conn_pool_t *pool = malloc(sizeof(conn_pool_t));
    init_pool(pool);

    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/

    int fd; /* socket descriptor */
    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        systemError("Creating socket failed");
    }

    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    int on = TURN_ON;
    ioctl(fd, (int)FIONBIO, (char *)&on);
    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    struct sockaddr_in srv; /* used by bind() */
    /* create the socket */
    srv.sin_family = AF_INET; /* use the Internet addr family */
    srv.sin_port = htons(port);
    /* bind: a client may connect to any of my addresses */
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    int b = bind(fd, (struct sockaddr *)&srv, sizeof(srv));
    if (b < 0) {
        systemError("Bind failed");
    }

    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    int l = listen(fd, MAX_TO_LISTEN);
    if (l < 0) {
        systemError("Listen failed");
    }

    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_SET(fd, &pool->read_set);
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do {
        // updating the maxfd in the pool in every run.
        conn_t *curr = pool->conn_head;
        while (curr != NULL) {
            if (curr->fd > pool->maxfd) {
                pool->maxfd = curr->fd;
            }
            curr = curr->next;
        }
        if (pool->maxfd < fd) {
            pool->maxfd = fd;
        }
        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/
        pool->ready_write_set = pool->write_set;
        pool->ready_read_set = pool->read_set;
        /**********************************************************/
        /* Call select()
         */
        /**********************************************************/
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        pool->nready = select(pool->maxfd + 1, &pool->ready_read_set,
                              &pool->ready_write_set, NULL, NULL);
        if (pool->nready < 0) {
            break;
        }

        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/

        int i;
        for (i = 0; i < pool->maxfd + 1 && pool->nready > 0; i++) {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all of the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(i, &pool->ready_read_set)) {
                /***************************************************/
                /* A descriptor was found that was readable		   */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again. */
                /****************************************************/
                char buff[BUFFER_SIZE] = "";
                if (i == fd) {
                    int newfdstream;
                    struct sockaddr_in acc;    /* returned by accept()*/
                    int acc_len = sizeof(acc); /* used by accept() and get the length of the address */
                    newfdstream = accept(i, (struct sockaddr *)&acc, &acc_len);
                    printf("New incoming connection on sd %d\n", newfdstream);
                    pool->nready--;
                    add_conn(newfdstream, pool);
                }
                    /****************************************************/
                    /* If this is not the listening socket, an 			*/
                    /* existing connection must be readable				*/
                    /* Receive incoming data his socket             */
                    /****************************************************/
                else {
                    printf("Descriptor %d is readable\n", i);
                    int readFlag = (int)read(i, buff, BUFFER_SIZE);
                    printf("%d bytes received from sd %d\n", (int)strlen(buff), i);
                    pool->nready--;
                    /* If the connection has been closed by client        */
                    /* remove the connection (remove_conn(...))           */
                    if (readFlag == TURN_OFF) {
                        printf("Connection closed for sd %d\n", i);
                        remove_conn(i, pool);
                    }
                    /* If the connection has been closed by client 		*/
                    /* remove the connection (remove_conn(...))    		*/

                    /**********************************************/
                    /* Data was received, add msg to all other    */
                    /* connectios */
                    /**********************************************/
                    add_msg(i, buff, (int)strlen(buff), pool);
                }
            } /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(i, &pool->ready_write_set)) {
                /* try to write all msgs in queue to sd */
                write_to_client(i, pool);
                pool->nready--;
            }
            /*******************************************************/

        } /* End of loop through selectable descriptors */

    } while (end_server == TURN_OFF);

    /*************************************************************/
    /* If we are here, Control-C was typed,
     */
    /* clean up all open connections */
    /*************************************************************/
    int i = 0;
    int remove[pool->nr_conns];  // get all active connection to remove later
    // the number of active connections at the same time
    int size = (int)pool->nr_conns;
    conn_t *current = pool->conn_head;
    while (current != NULL) {
        remove[i] = current->fd;
        current = current->next;
        i++;
    }
    for (i = size; i > 0; i--) {
        remove_conn(remove[i - 1], pool);
    }
    free(pool);
    return EXIT_SUCCESS;
}
//helpter functions to implement the chat server
int init_pool(conn_pool_t *pool) {
    // init all fields
    pool->nready = 0;
    pool->maxfd = 0;
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    pool->nr_conns = 0;
    pool->conn_head = NULL;
    return 0;
}

int add_conn(int sd, conn_pool_t *pool) {
    /*
     * 1. allocate connection and init fields
     * 2. add connection to pool
     * */
    conn_t *new = malloc(sizeof(conn_t));
    if (new == NULL) {
        return -1;
    }
    new->fd = sd;
    new->prev = NULL;
    new->next = NULL;
    new->write_msg_head = NULL;
    new->write_msg_tail = NULL;
    if (pool->conn_head != NULL) {
        new->next = pool->conn_head;
        pool->conn_head->prev = new;
    }
    pool->conn_head = new;
    FD_SET(sd, &pool->read_set);
    pool->nr_conns++;
    return 0;
}

int remove_conn(int sd, conn_pool_t *pool) {
    /*
     * 1. remove connection from pool
     * 2. deallocate connection
     * 3. remove from sets
     * 4. update max_fd if needed
     */

    printf("removing connection with sd %d \n", sd);
    conn_t *p = pool->conn_head;
    conn_t *tmp = NULL;
    while (p != NULL) {
        if (p->fd == sd) {
            tmp = p;
            break;
        } else
            p = p->next;
    }
    if (tmp == NULL) {
        return -1;
    }
    if (tmp == pool->conn_head) {
        pool->conn_head = tmp->next;
    }
    if (tmp->next != NULL) {
        tmp->next->prev = tmp->prev;
    }
    if (tmp->prev != NULL) {
        tmp->prev->next = tmp->next;
    }
    FD_CLR(sd, &pool->write_set);
    FD_CLR(sd, &pool->read_set);
    pool->nr_conns--;
    close(sd);
    free(tmp);
    return 0;
}

int add_msg(int sd, char *buffer, int len, conn_pool_t *pool) {
    /*
     * 1. add msg_t to write queue of all other connections
     * 2. set each fd to check if ready to write
     */

    conn_t *add = pool->conn_head;
    while (add != NULL) {
        if (add->fd != sd) {
            msg_t *msg = malloc(sizeof(msg_t));
            if (msg == NULL) {
                return -1;
            }
            msg->message = (char *)(malloc(sizeof(char) * len + 1));
            if (msg->message == NULL) {
                return -1;
            }
            strcpy(msg->message, buffer);
            msg->message[len + 1] = '\0';
            msg->size = len;
            msg->next = NULL;
            msg->prev = NULL;

            if (add->write_msg_head == NULL) {
                add->write_msg_head = msg;
            } else {
                msg_t *tmpMSG = add->write_msg_head;
                while (tmpMSG->next != NULL) {
                    tmpMSG = tmpMSG->next;
                }
                tmpMSG->next = msg;
                msg->prev = add->write_msg_tail;
                msg->next = NULL;
            }
            add->write_msg_tail = msg;
            FD_SET(add->fd, &pool->write_set);
        }
        add = add->next;
    }

    return 0;
}

int write_to_client(int sd, conn_pool_t *pool) {
    /*
     * 1. write all msgs in queue
     * 2. deallocate each writen msg
     * 3. if all msgs were writen successfully, there is nothing else to write to
     * this fd... */

    conn_t *curr = pool->conn_head;
    msg_t *tmp;
    msg_t *prev;
    while (curr != NULL) {
        if (curr->fd == sd) {
            tmp = curr->write_msg_head;
            if (tmp == NULL) {
                curr = curr->next;
                continue;
            }
            while (tmp != NULL) {
                write(sd, tmp->message, tmp->size);
                prev = tmp;//set prev
                tmp = tmp->next;
                free(prev->message);
                free(prev);
            }

            curr->write_msg_head = NULL;
            curr->write_msg_tail = NULL;
        }
        curr = curr->next;
    }
    FD_CLR(sd, &pool->write_set);  // clear
    return 0;
}
// print usage error
void printUsage() {
    printf("Usage: chatServer <port>");  // Command line usage
}
void systemError(char *str)  // return error msg
{
    perror(str);
    exit(EXIT_FAILURE);
}