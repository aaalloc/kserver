#include <argp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 12345
#define DEFAULT_NUM_REQUESTS 1000
#define DEFAULT_NUM_THREADS 50
#define BUFFER_SIZE 1024
#define END_FLAG 0x1337 // Magic number to indicate end of response

// Structure pour les arguments du programme
struct arguments_t
{
    char *host;
    int port;
    int num_requests;
    int num_threads;
};

static struct arguments_t arguments;

// Structure pour passer les données aux threads
struct thread_data
{
    char *host;
    int port;
    int requests_per_thread;
    double avg_response_time;
};

// Variables globales pour argp
const char *argp_program_version = "stresstest 1.0";
static char doc[] = "Stress test kserver";
static char args_doc[] = "";

static struct argp_option options[] = {
    {"host", 'h', "HOST", 0, "Host of the server to stress test", 0},
    {"port", 'p', "PORT", 0, "Port of the server to stress test", 0},
    {"num-requests", 'r', "NUM", 0, "Total number of requests to send", 0},
    {"num-threads", 't', "NUM", 0, "Number of threads to use for sending requests", 0},
    {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments_t *arguments = state->input;

    switch (key)
    {
    case 'h':
        arguments->host = arg;
        break;
    case 'p':
        arguments->port = atoi(arg);
        break;
    case 'r':
        arguments->num_requests = atoi(arg);
        break;
    case 't':
        arguments->num_threads = atoi(arg);
        break;
    case ARGP_KEY_ARG:
        argp_usage(state);
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

double inline get_time_diff(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int connect_to_server(char *addr, int port)
{
    struct addrinfo *res = NULL;
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    int err = getaddrinfo(addr, port_str, &hints, &res);
    if (err != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return -1;
    }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        fprintf(stderr, "Connection failed: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    return sock;
}

void *send_request(void *arg)
{
    struct thread_data *data = (struct thread_data *)arg;
    double *response_times = calloc(data->requests_per_thread, sizeof(double));
    if (!response_times)
    {
        fprintf(stderr, "Failed to allocate memory for response times\n");
        data->avg_response_time = -1.0;
        return NULL;
    }

    int valid_responses = 0;

    int sock = connect_to_server(data->host, data->port);
    if (sock < 0)
    {
        fprintf(stderr, "Failed to connect to server %s:%d\n", data->host, data->port);
        free(response_times);
        data->avg_response_time = -1.0; // Indicate failure
        return NULL;
    }

    const char *msg = "Hello, server!";
    ssize_t msg_len = strlen(msg);
    for (int i = 0; i < data->requests_per_thread; i++)
    {
        struct timespec start_time, end_time;
        uint16_t buffer[BUFFER_SIZE];

        clock_gettime(CLOCK_MONOTONIC, &start_time);

        if (send(sock, msg, msg_len, 0) < 0)
        {
            fprintf(stderr, "Send failed: %s\n", strerror(errno));
            continue;
        }

    receive:;
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0)
        {
            fprintf(stderr, "Receive failed: %s\n", strerror(errno));
            continue;
        }
        if ((bytes_received == 2 && buffer[0] == END_FLAG) || buffer[(bytes_received / 2) - 1] == END_FLAG)
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            response_times[valid_responses] = get_time_diff(start_time, end_time);
            valid_responses++;
            continue; // Valid response received
        }

        goto receive; // Continue receiving until we get END_FLAG
    }

    if (close(sock) < 0)
    {
        fprintf(stderr, "Socket close failed: %s\n", strerror(errno));
    }

    if (valid_responses > 0)
    {
        double sum = 0.0;
        for (int i = 0; i < valid_responses; i++)
            sum += response_times[i];

        data->avg_response_time = sum / valid_responses;

        printf("Average response time: %.4f seconds (valid responses: %d/%d)\n", data->avg_response_time,
               valid_responses, data->requests_per_thread);
    }
    else
    {
        printf("No valid responses received\n");
        data->avg_response_time = -1.0; // Indicate failure
    }

    free(response_times);
    return NULL;
}

void stress_test(char *host, int port, int num_requests, int num_threads)
{
    pthread_t threads[num_threads];
    struct thread_data thread_data_array[num_threads];
    double *avg_times = malloc(num_threads * sizeof(double));

    int requests_per_thread = num_requests / num_threads;

    printf("Starting stress test on %s:%d with %d requests using %d threads...\n", host, port, num_requests,
           num_threads);

    for (int i = 0; i < num_threads; i++)
    {
        thread_data_array[i].host = host;
        thread_data_array[i].port = port;
        thread_data_array[i].requests_per_thread = requests_per_thread;

        if (pthread_create(&threads[i], NULL, send_request, &thread_data_array[i]) != 0)
            fprintf(stderr, "Failed to create thread %d\n", i);
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    double avg_time_sum = 0.0;
    for (int i = 0; i < num_threads; i++)
    {
        avg_times[i] = thread_data_array[i].avg_response_time;
        if (avg_times[i] >= 0.0)
            avg_time_sum += avg_times[i];
    }
    double overall_avg_time = avg_time_sum / num_threads;
    printf("Overall average response time: %.4f seconds\n", overall_avg_time);
}

int main(int argc, char **argv)
{

    arguments.host = DEFAULT_HOST;
    arguments.port = DEFAULT_PORT;
    arguments.num_requests = DEFAULT_NUM_REQUESTS;
    arguments.num_threads = DEFAULT_NUM_THREADS;

    error_t err = argp_parse(&argp, argc, argv, 0, 0, &arguments);
    if (err != 0)
    {
        fprintf(stderr, "Error parsing arguments: %s\n", strerror(err));
        return err;
    }
    stress_test(arguments.host, arguments.port, arguments.num_requests, arguments.num_threads);

    return 0;
}