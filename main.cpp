#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

/**
  Connection handler - this will be executed in
  the new process, after forking, and it will read
  all the data from the socket, while available and
  to echo it on the local terminal.

  Params:
    sd = socket to the client
*/
#define BUF_SIZE (1024)

int echo_client(int sd) 
{
    int result = 0; 

    char buf[BUF_SIZE + 1] = {0};

    ssize_t n_read;
    while (0 < (n_read = read(sd, buf, BUF_SIZE))) 
    {
        buf[n_read] = '\0';
        printf("%s\n", buf);
    }

    if (0 > n_read)
    {
      perror("read() failed");
      result = -1;
    }
    else
    {
      fprintf(stderr, "The other side orderly shut down the connection.\n");
    }

    close(sd);

    return result;
}


int main(void)
{
    // Create a listening socket
    int listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening_socket == -1)
    {
        perror("socket() failed");
        return EXIT_FAILURE;
    }

    // Bind it to port 15000.
    unsigned short listening_port = 15000;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listening_port);

    socklen_t sock_len = sizeof(addr);

    if (0 > (bind(listening_socket, (const struct sockaddr*) &addr, sock_len)))
    {
        perror("bind() failed");
        return EXIT_FAILURE;
    }

    // Start listening
    if (0 > listen(listening_socket, 0))
    {
        perror("listen() failed");
        return EXIT_FAILURE;
    }

    // Accept new connections, fork the new process for handling
    // and handle the connection in the new process, while the parent
    // is waiting for another connection to arrive.
    int accepted_socket = 0;
    while (0 < (accepted_socket =
                  accept(listening_socket, (struct sockaddr*) &addr, &sock_len)))
    {
        pid_t pid_child = fork();

        if (0 > pid_child)
        {
            perror("fork() failed");
            return EXIT_FAILURE;
        }
        else if (0 == pid_child)
        {
            // inside the forked child here
            close(listening_socket); // The child does not need this any more. 

            echo_client(accepted_socket);

            return EXIT_SUCCESS;
        }

        // Inside parent process, since file descriptors are reference
        // counted, we need to close the client socket
        close(accepted_socket);
    }

    if (0 > accepted_socket)
    {
        perror("accept() failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
