#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define error(...)  {fprintf(stderr, __VA_ARGS__);}
#define error2(...) {error(__VA_ARGS__); exitcode = EXIT_FAILURE; goto exit;}
#define error3(...) {perror("ERROR");error(__VA_ARGS__); exitcode = EXIT_FAILURE; goto exit;}
#define error4(...) {perror("ERROR");error(__VA_ARGS__);}

#define info(...) {if (verbose_messages >= 1) {fprintf(stdout, __VA_ARGS__);}}
#define debug(...) {if (verbose_messages >= 2) {fprintf(stdout, __VA_ARGS__);}}
#define trace(...) {if (verbose_messages >= 3) {fprintf(stdout, __VA_ARGS__);}}

#define summary(...) fprintf(stdout, __VA_ARGS__)

#define VERSION   "0.1"
#define COPYRIGHT "Copyright (C) 2011 Michael Costello, Wandenberg Peixoto <wandenberg@gmail.com>"
#define DESCRIPTION_PUBLISHER "'%s' v%s - program to publish messages to test Push Stream Module.\n%s\n"
#define DESCRIPTION_SUBSCRIBER "'%s' v%s - program to subscribe channels to test Push Stream Module.\n%s\n"

#define DEFAULT_NUM_MESSAGES    1
#define DEFAULT_CONCURRENT_CONN 1
#define DEFAULT_NUM_CHANNELS    1
#define DEFAULT_SERVER_HOSTNAME "127.0.0.1"
#define DEFAULT_SERVER_PORT     9080
#define DEFAULT_TIMEOUT         1000

#define MAX_EVENTS (60000 * 8)
#define ITERATIONS_TILL_SUMMARY_PER_TIMEOUT 10000 //timeout: 1000 -> summary each 10 seconds
#define BUFFER_SIZE 1024
#define BIG_BUFFER_SIZE 640000

typedef struct
{
    long requested_connections;
    long connections;
    long messages;
    long bytes_written;
    long bytes_read;
} Statistics;

enum State {INIT=0, CONNECTING, CONNECTED, CLOSED};

// store per connection state here
typedef struct
{
    int		   index;
    int        main_sd;
    int		   sd;
    int		   message_count;
    int		   num_messages;
    int		   channel_count;
    long	   channel_id;
    long	   channel_start;
    long	   channel_end;
    char       content_buffer[BUFFER_SIZE];
    int        content_length;
    enum State state;
    struct sockaddr_in *server_address;
} Connection;

static int verbose_messages = 0;

int fill_server_address(const char *server_hostname, int server_port, struct sockaddr_in *server_address);
Connection *init_connections(int count, struct sockaddr_in *server_address, int main_sd);
int open_connection(Connection *connection);
void close_connection(Connection *connection);
int reopen_connection(Connection *connection);
int write_connection(Connection *connection, Statistics *stats, char *buffer, int buffer_len);
int change_connection(Connection *connection, uint32_t events);
float calc_message_per_second(int num_messages, int start_time);


#endif /* _UTIL_H_ */
