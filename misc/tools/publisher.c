/*
Copyright (C) 2011 Michael Costello, Wandenberg Peixoto <wandenberg@gmail.com>

Usage './publisher --help' to see option
*/
#include <argtable2.h>
#include "util.h"


void write_message(Connection *connection, Statistics *stats);
void read_response(Connection *connection, Statistics *stats, char *buffer, int buffer_len);

int
main_program(int num_messages, int num_channels, int num_connections, int loop_interval, const char *server_hostname, int server_port, int timeout)
{
    struct sockaddr_in server_address;
    int main_sd = -1, num_events = 0, i, event_mask, channels_per_connection, num, start_time = 0, iters_to_next_summary = 0;
    Connection *connections = NULL, *connection;
    Statistics stats = {0,0,0,0,0};
    int exitcode = EXIT_SUCCESS;
    struct epoll_event events[MAX_EVENTS];
    char buffer[BIG_BUFFER_SIZE];

    info("Publisher starting up\n");
    info("Publish: %d messages to %d channels on server: %s:%d\n", num_messages, num_channels, server_hostname, server_port);

    if ((fill_server_address(server_hostname, server_port, &server_address)) != 0) {
        error2("ERROR host name not found\n");
    }

    if ((main_sd = epoll_create(200 /* this size is not used on Linux kernel 2.6.8+ */)) < 0) {
        error3("Failed %d creating main epoll socket\n", errno);
    }

    if ((connections = init_connections(num_connections, &server_address, main_sd)) == NULL) {
        error2("Failed to create to connections\n");
    }

    stats.requested_connections = num_connections;

    channels_per_connection = num_channels / num_connections;
    for (i = 0; i < num_connections; i++) {
        num = i * channels_per_connection;
        connections[i].channel_start = num;
        num += channels_per_connection - 1;
        connections[i].channel_end = ((num > num_channels) || (i == (num_connections - 1))) ? num_channels - 1 : num;
    }

    // infinite loop
    debug("Entering Infinite Loop\n");

    iters_to_next_summary = ITERATIONS_TILL_SUMMARY_PER_TIMEOUT/timeout;

    for(;;) {
        if ((num_events = epoll_wait(main_sd, events, MAX_EVENTS, timeout)) < 0) {
            error3("epoll_wait failed\n");
        }

        for (i = 0; i < num_events; i++) {
            event_mask = events[i].events;
            connection = (Connection *)(events[i].data.ptr);

            if ((connection->message_count >= num_messages) && (connection->channel_id > connection->channel_end)) {
                // remove write flag from event
                if (change_connection(connection, EPOLLIN | EPOLLHUP) < 0) {
                    error2("Failed creating socket for connection = %d\n", connection->index);
                }

                if (event_mask & EPOLLOUT) { // WRITE
                    continue;
                }
            }

            if (event_mask & EPOLLHUP) { // SERVER HUNG UP
                debug("EPOLLHUP\n");
                info("Server hung up on conncetion %d. Reconecting...\n", connection->index);
                sleep(1);
                reopen_connection(connection);

                continue;
            }

            if (event_mask & EPOLLERR) {
                debug("EPOLLERR\n");
                info("Server returned an error on connection %d. Reconecting...\n", connection->index);
                reopen_connection(connection);

                continue;
            }

            if (event_mask & EPOLLIN) { // READ
                debug("----------READ AVAILABLE-------\n");

                if (connection->state == CONNECTED) {
                    read_response(connection, &stats, buffer, BIG_BUFFER_SIZE);
                }
            }

            if (event_mask & EPOLLOUT) { // WRITE
                debug("----------WRITE AVAILABLE-------\n");

                if (start_time == 0) {
                    start_time = time(NULL);
                }

                if (connection->state == CONNECTING) {
                    connection->state = CONNECTED;
                    stats.connections++;
                    debug("Connection opened for index=%d\n", connection->index);
                }

                write_message(connection, &stats);
            }
        }

        if (iters_to_next_summary-- <= 0) {
            iters_to_next_summary = ITERATIONS_TILL_SUMMARY_PER_TIMEOUT/timeout;
            summary("Connections=%ld, Messages=%ld BytesWritten=%ld Msg/Sec=%0.2f\n", stats.connections, stats.messages, stats.bytes_written, calc_message_per_second(stats.messages, start_time));
        }

        if (stats.messages >= (num_channels * num_messages)) {
            summary("Connections=%ld, Messages=%ld BytesWritten=%ld Msg/Sec=%0.2f\n", stats.connections, stats.messages, stats.bytes_written, calc_message_per_second(stats.messages, start_time));
            if (loop_interval != 0) {
                stats.messages = 0;
                for (i = 0; i < num_connections; i++) {
                    connections[i].message_count = 0;
                    connections[i].channel_id = -1;

                    if (change_connection(&connections[i], EPOLLIN | EPOLLHUP | EPOLLOUT) < 0) {
                        error2("Failed creating socket for connection = %d\n", connection->index);
                    }
                }

                sleep(loop_interval);

            } else {
                for (i = 0; i < num_connections; i++) {
                    close_connection(&connections[i]);
                    stats.connections--;
                }
                exitcode = EXIT_SUCCESS;
                goto exit;
            }
        }
    }


exit:
    if (connections != NULL) free(connections);

    return exitcode;
}


void
write_message(Connection *connection, Statistics *stats)
{
    char buffer[BUFFER_SIZE];
    int len = 0, bytes_written = 0;

    if ((connection->channel_id <= connection->channel_start) || (connection->channel_id > connection->channel_end)) {
        connection->channel_id = connection->channel_start;
        connection->message_count++;

        // gives a message payload of 140 bytes
        connection->content_length = sprintf(connection->content_buffer, "**MSG** msg=%06d 012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789", connection->message_count);
        connection->content_buffer[connection->content_length] = '\0';
    }

    len = sprintf(buffer, "POST /pub?id=my_channel_%ld HTTP/1.1\r\nHost: loadtest\r\nContent-Length: %d\r\n\r\n%s", connection->channel_id, connection->content_length, connection->content_buffer);

    if (write_connection(connection, stats, buffer, len) == EXIT_FAILURE) {
        reopen_connection(connection);
        return;
    }

    connection->channel_id++;
}


void
read_response(Connection *connection, Statistics *stats, char *buffer, int buffer_len)
{
    int bytes_read = 0, bad_count = 0, ok_count = 0;

    bytes_read = read(connection->sd, buffer, buffer_len - 1);

    if (bytes_read < 0) {
        error("Error reading from socket for connection %d\n", connection->index);
        reopen_connection(connection);
        return;
    }

    if (bytes_read == 0) { // server disconnected us
        // reconnect
        info("Server disconnected as requested %d.\n", connection->index);
        close_connection(connection);
        return;
    }

    stats->bytes_read += bytes_read;
    buffer[bytes_read] = '\0';
    debug("Read %d bytes\n", bytes_read);
    trace("Read Message: %s\n", buffer);

    bad_count = count_strinstr(buffer, "HTTP/1.1 4");
    bad_count += count_strinstr(buffer, "HTTP/1.1 5");

    if (bad_count > 0) {
        info("Recevied error. Buffer is %s\n", buffer);
        reopen_connection(connection);
        return;
    }

    ok_count = count_strinstr(buffer, "HTTP/1.1 200 OK");
    stats->messages += ok_count;
}


int
main(int argc, char **argv)
{
    struct arg_int *messages    = arg_int0("m", "messages", "<n>", "define number of messages to publish in each channel (default is 1)");
    struct arg_int *channels  = arg_int0("c", "channels", "<n>", "define number of channels (default is 1)");
    struct arg_int *publishers  = arg_int0("p", "publishers", "<n>", "define number of publishers (default is 1)");

    struct arg_int *loop_interval  = arg_int0("l", "loop_interval", "<n>", "define the interval between loops in seconds. each loop send the specified number of messages (default is 0, not loop)");

    struct arg_str *server_name = arg_str0("S", "server", "<hostname>", "server hostname where messages will be published (default is \"127.0.0.1\")");
    struct arg_int *server_port = arg_int0("P", "port", "<n>", "server port where messages will be published (default is 9080)");

    struct arg_int *timeout = arg_int0(NULL, "timeout", "<n>", "timeout when waiting events on communication to the server (default is 1000)");
    struct arg_int *verbose = arg_int0("v", "verbose", "<n>", "increase output messages detail (0 (default) - no messages, 1 - info messages, 2 - debug messages, 3 - trace messages");

    struct arg_lit *help    = arg_lit0(NULL, "help", "print this help and exit");
    struct arg_lit *version = arg_lit0(NULL, "version", "print version information and exit");
    struct arg_end *end     = arg_end(20);

    void* argtable[] = { messages, channels, publishers, loop_interval, server_name, server_port, timeout, verbose, help, version, end };

    const char* progname = "publisher";
    int nerrors;
    int exitcode = EXIT_SUCCESS;

    /* verify the argtable[] entries were allocated sucessfully */
    if (arg_nullcheck(argtable) != 0) {
        /* NULL entries were detected, some allocations must have failed */
        printf("%s: insufficient memory\n", progname);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    /* set any command line default values prior to parsing */
    messages->ival[0] = DEFAULT_NUM_MESSAGES;
    publishers->ival[0] = DEFAULT_CONCURRENT_CONN;
    channels->ival[0] = DEFAULT_NUM_CHANNELS;
    loop_interval->ival[0] = 0;
    server_name->sval[0] = DEFAULT_SERVER_HOSTNAME;
    server_port->ival[0] = DEFAULT_SERVER_PORT;
    timeout->ival[0] = DEFAULT_TIMEOUT;
    verbose->ival[0] = 0;

    /* Parse the command line as defined by argtable[] */
    nerrors = arg_parse(argc, argv, argtable);

    /* special case: '--help' takes precedence over error reporting */
    if (help->count > 0) {
        printf(DESCRIPTION_PUBLISHER, progname, VERSION, COPYRIGHT);
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = EXIT_SUCCESS;
        goto exit;
    }

    /* special case: '--version' takes precedence error reporting */
    if (version->count > 0) {
        printf(DESCRIPTION_PUBLISHER, progname, VERSION, COPYRIGHT);
        exitcode = EXIT_SUCCESS;
        goto exit;
    }

    if (publishers->ival[0] > channels->ival[0]) {
        printf("Publisher number (%d) can not be greater than Channel number (%d).\n", publishers->ival[0], channels->ival[0]);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    /* If the parser returned any errors then display them and exit */
    if (nerrors > 0) {
        /* Display the error details contained in the arg_end struct.*/
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    verbose_messages = verbose->ival[0];

    /* normal case: take the command line options at face value */
    exitcode = main_program(messages->ival[0], channels->ival[0], publishers->ival[0], loop_interval->ival[0], server_name->sval[0], server_port->ival[0], timeout->ival[0]);

exit:
    /* deallocate each non-null entry in argtable[] */
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    return exitcode;
}
