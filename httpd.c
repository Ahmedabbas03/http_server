#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* Our server program is going to follow a process of:
    1. Create a listen socket
    2. Accept a client
    3. Create a client thread and Process requests
    4. Loop infinitely and return to (2)
 */

// Function that creates a listen socket
int create_listen_socket(int *socket_fd, char const *port);
int accept_client(int listen_socket_fd);

/*
 * argv[1] => Port number
 */
int main(int argc, char *argv[])
{
    if (argc != 2)
        errx(1, "Usage: %s PORTNUM", argv[0]);

    int listen_socket;
    int status = create_listen_socket(&listen_socket, argv[1]);

    switch (status)
    {
    case 0:
        break;
    case EAI_SYSTEM:
        err(1, "create_listen_socket");
    default:
        errx(1, "create_listen_socket:  %s", gai_strerror(status));
    }

    for (;;)
    {
        accept_client(listen_socket);
    }
}

int create_listen_socket(int *socket_fd, char const *port)
{
    socklen_t peer_addrlen;
    struct addrinfo *result, *rp;

    {
        struct addrinfo hints =
            {
                .ai_family = AF_UNSPEC,
                .ai_socktype = SOCK_STREAM,
                .ai_flags = AI_PASSIVE};

        int status = getaddrinfo(NULL, port, &hints, &result);
        if (status != 0)
            return status;
    }

    int sfd;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */
        close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL)
        return EAI_SYSTEM;

    if (listen(sfd, 128) < 0)
        return EAI_SYSTEM;

    *socket_fd = sfd;

    return 0;
}

struct client_targs
{
    int fd;
};

void *client_thread(void *p)
{
    struct client_targs *args = p;
    int fd = args->fd;

    FILE *fp = fdopen(fd, "r");
    if (!fp)
        goto err;
    setbuf(fp, 0);

    char *line = 0;
    size_t n = 0;
    ssize_t ret = getline(&line, &n, fp);
    if (ret < 0)
        goto err;

    // resource:
    char *resource;
    int maj_version, min_version;
    int n_matches = sscanf(line, "GET %ms HTTP/%d.%d", &resource, &maj_version, &min_version);
    free(line);

    if (n_matches == 1 || n_matches == 3)
    {
        FILE *rfp = fopen(resource, "r");

        if (n_matches == 1)
            goto send_body;

        fprintf(fp, "HTTP/%d.%d", maj_version, min_version);

        if (rfp)
            fputs("200 OK\r\n", fp);
        else
            fputs("404 Not Found\r\n", fp);

        fputs("\r\n", fp);

    send_body:;
        if (rfp)
        {
            for (;;)
            {
                char buf[BUFSIZ];
                size_t n = fread(buf, 1, sizeof buf, rfp);
                if (n == 0)
                    break;
                fwrite(buf, 1, n, fp);
                if (n < sizeof buf)
                    break;
            }
            fclose(rfp);
        }
        else
        {
            fprintf(fp, "<html><body><h1>404 Not Found</h1><p>The resource `%s` could not be found on the server.</p></body></html>\r\n", resource);
        }
    }
    else
    {
        fputs("HTTP/0.9 501 Not Implemented\r\n", fp);
    }

    fflush(fp);

    if (shutdown(fd, SHUT_WR) < 0)
        close(fd);

    while (fgetc(fp) != EOF)
        ;

err:
    if (fp)
        fclose(fp);
    else
        close(fd);

    free(p);
    return 0;
}

/*Fancy read that reasdd from sockets and gets all the info it needs for incoming client conenction and creates new socket*/
int accept_client(int listen_socket_fd)
{
    int client_fd = accept(listen_socket_fd, 0, 0);
    if (client_fd < 0)
        return -1;

    pthread_t cli_thread;

    struct client_targs *client_args = malloc(sizeof *client_args);
    if (!client_args)
    {
        close(client_fd);
        return -1;
    }
    client_args->fd = client_fd;

    pthread_create(&cli_thread, 0, client_thread, client_args);
    pthread_detach(cli_thread);

    return 0;
}
