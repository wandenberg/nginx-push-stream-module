#include "util.h"


int
fill_server_address(const char *server_hostname, int server_port, struct sockaddr_in *server_address)
{
    struct hostent *server = NULL;
    if ((server = gethostbyname(server_hostname)) == NULL) {
        return EXIT_FAILURE;
    }

    bzero((char *) server_address, sizeof(struct sockaddr_in));
    server_address->sin_family = AF_INET;
    memcpy((char *) &server_address->sin_addr.s_addr, (const char *) server->h_addr, server->h_length);
    server_address->sin_port = htons(server_port);

    return EXIT_SUCCESS;
}


Connection *
init_connections(int num_connections, struct sockaddr_in *server_address, int main_sd)
{
    Connection *connections;
    int         i;

    if ((connections = (Connection *) malloc(sizeof(Connection) * num_connections)) == NULL) {
        return NULL;
    }

    for (i = 0; i < num_connections; ++i) {
        connections[i].index = i;
        connections[i].server_address = server_address;
        connections[i].main_sd = main_sd;
        if (open_connection(&connections[i]) != 0) {
            error("Opening connection %d\n", i);
            return NULL;
        }
    }

    info("Added %d connections.\n", num_connections);

    return connections;
}


int
open_connection(Connection *connection)
{
    struct epoll_event anEvent;
    int exitcode = EXIT_SUCCESS;

    connection->state = CONNECTING;
    connection->channel_id = -1;
    connection->content_length = 0;
    connection->channel_count = 0;

    if ((connection->sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error3("ERROR %d opening socket for connection %d\n", errno, connection->index);
    }

//    // set nonblocking
//    int flags = fcntl(connection->sd, F_GETFL, 0);
//    fcntl(connection->sd, F_SETFL, flags | O_NONBLOCK);
//
//    int rc = connect(connection->sd, (struct sockaddr *) connection->server_address, sizeof(struct sockaddr_in));
//    if ((rc < 0) && (errno != EINPROGRESS))  {
//        error3("ERROR connecting to server on connection %d\n", connection->index);
//    }

    if (connect(connection->sd, (struct sockaddr *) connection->server_address, sizeof(struct sockaddr_in)) < 0)  {
        error3("ERROR connecting to server on connection %d\n", connection->index);
    }

    debug("Adding connection %d\n", connection->index);

    anEvent.events = EPOLLIN | EPOLLOUT | EPOLLHUP;
    anEvent.data.ptr = (void *) connection;
    if (epoll_ctl(connection->main_sd, EPOLL_CTL_ADD, connection->sd, &anEvent) < 0)	{
        error3("ERROR %d Failed creating socket for connection %d\n", errno, connection->index);
    }

    debug("Connection opening for index %d\n", connection->index);

exit:
    return exitcode;
}


void
close_connection(Connection *connection)
{
    connection->state = CLOSED;
    close(connection->sd);
}



int
reopen_connection(Connection *connection)
{
    close_connection(connection);
    return open_connection(connection);
}


int
change_connection(Connection *connection, uint32_t events)
{
    struct epoll_event anEvent;
    anEvent.events = events;
    anEvent.data.ptr = (void *) connection;
    return epoll_ctl(connection->main_sd, EPOLL_CTL_MOD, connection->sd, &anEvent);
}


int
write_connection(Connection *connection, Statistics *stats, char *buffer, int buffer_len)
{
    int bytes_written = 0;

    bytes_written = write(connection->sd, buffer, buffer_len);

    if (bytes_written != buffer_len) {
        error4("Error %d writing bytes (wrote=%d, wanted=%d) for connection %d\n", errno, bytes_written, buffer_len, connection->index);
        return EXIT_FAILURE;
    }
    stats->bytes_written += bytes_written;
    trace("Wrote %s\n", buffer);

    return EXIT_SUCCESS;
}


float
calc_message_per_second(int num_messages, int start_time)
{
    float ret_val = 0.0;
    int now = time(NULL);
    int diff = now - start_time;

    if (diff == 0) {
        diff = 1;
    }

    ret_val = (float) num_messages/diff;

    info("CALC TIME.  Messages=%d, Time=%d Avg=%0.2f\n", num_messages, diff, ret_val);

    return ret_val;
}


int
count_strinstr(const char *big, const char *little)
{
    const char *p;
    int count = 0;
    size_t lil_len = strlen(little);

    /* you decide what to do here */
    if (lil_len == 0)
        return -1;

    p = strstr(big, little);
    while (p) {
    count++;
    p = strstr(p + lil_len, little);
    }
    return count;
}

