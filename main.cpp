#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <signal.h>

/**
  Connection handler - this will be executed in
  the new process, after forking, and it will read
  all the data from the socket, while available and
  to echo it on the local terminal.

  Params:
    sd = socket to the client
*/
#define BUF_SIZE (1024)

void sig_handler(int sig)
{
    static int count = 3;
    if (sig == SIGINT) {
        std::cout << "received SIGINT\n";
        if (--count <= 0) abort();
    }
}

/*!
 * \brief Call a function and retry if an EINTR error is encountered.
 * \param f The function to retry.
 * \param error_value The error value returned by the call on retry failure.
 * \return The return code returned by function f or error_value on retry failure.
 */
template <typename FUNC>
ssize_t RetryCallOnEINTR(FUNC f, std::string call) {
  size_t retry_count = 8;
  ssize_t rc = -1;
  std::cout << "waiting on call " << call << "\n";
  while (--retry_count && (rc = f()) == -1 && errno == EINTR) {
    std::cout << "EINTR " << call << "\n";
  }
  std::cout << "call done\n";
  return rc;
}

int echo_client(int sd)
{
    int result = 0;

    char buf[BUF_SIZE + 1] = {0};

    ssize_t n_read;
    while ((n_read = RetryCallOnEINTR([&](){ return recv(sd, buf, BUF_SIZE, 0); }, "recv")) > 0)
    {
        buf[n_read] = '\0';
        printf("got %s\n", buf);
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
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        printf("\ncan't catch SIGINT\n");
    }

    if (siginterrupt(SIGINT, 1) == -1) {
      perror("siginterrupt");
    }

    // Create a listening socket
    int listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening_socket == -1)
    {
        perror("socket() failed");
        return EXIT_FAILURE;
    }

    // Bind it to port 15000.
    unsigned short listening_port = 15001;

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
    int accepted_socket = 1;
    while(accepted_socket > 0)
    {
        std::cout << "main thread accepting connection now\n";
        int accepted_socket = RetryCallOnEINTR([&](){
           return accept(listening_socket, (struct sockaddr*) &addr, &sock_len);
        }, "accept");
        std::cout << "main thread got a connection\n";

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
