#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include "simplehttp/queue.h"
#include "simplehttp/simplehttp.h"
#include "simplehttp/uthash.h"

#define VERSION "1.2"

struct queue_entry {
    TAILQ_ENTRY(queue_entry) entries;
    size_t bytes;
    char data[1];
};

struct NamedQueue {
    char *name;
    uint64_t n_gets;
    uint64_t n_puts;
    uint64_t depth;
    uint64_t depth_high_water;
    TAILQ_HEAD(, queue_entry) queue_data;
    UT_hash_handle hh;
};

char *progname = "simplequeue";
char *overflow_log = NULL;
FILE *overflow_log_fp = NULL;
uint64_t max_depth = 0;
size_t   max_bytes = 0;
uint64_t total_depth = 0;
uint64_t total_depth_high_water = 0;
uint64_t n_puts = 0;
uint64_t n_gets = 0;
uint64_t n_overflow = 0;
size_t   n_bytes = 0;
struct NamedQueue *named_queues = NULL;

struct NamedQueue *get_or_create_queue(char *queue_name) {
    struct NamedQueue *queue;
    HASH_FIND_STR(named_queues, queue_name, queue);
    if (!queue) {
        queue = malloc(sizeof(struct NamedQueue));
        queue->n_gets = 0;
        queue->n_puts = 0;
        queue->depth = 0;
        queue->depth_high_water = 0;
        queue->name = strdup(queue_name);
        TAILQ_INIT(&(queue->queue_data));
        HASH_ADD_KEYPTR(hh, named_queues, queue->name, strlen(queue->name), queue);
    }
    return queue;
}

void
hup_handler(int signum)
{
    signal(SIGHUP, hup_handler);
    if (overflow_log_fp) {
        fclose(overflow_log_fp);
    }
    if (overflow_log) {
        overflow_log_fp = fopen(overflow_log, "a");
        if (!overflow_log_fp) {
            perror("fopen failed: ");
            exit(1);
        }
        fprintf(stdout, "opened overflow_log: %s\n", overflow_log);
    }
}

void
overflow_one(struct NamedQueue *queue)
{
    struct queue_entry *entry;

    entry = TAILQ_FIRST(&(queue->queue_data));
    if (entry != NULL) {
        TAILQ_REMOVE(&(queue->queue_data), entry, entries);
        fwrite(entry->data, entry->bytes, 1, overflow_log_fp);
        fwrite("\n", 1, 1, overflow_log_fp);
        n_bytes -= entry->bytes;
        total_depth--;
        queue->depth--;
        n_overflow++;
        free(entry);
    }
}

void
stats(struct evhttp_request *req, struct evbuffer *evb, void *ctx)
{
    struct evkeyvalq args;
    const char *reset;
    const char *format;
    int comma_flag;
    struct NamedQueue *queue, *tmp_queue;
    
    evhttp_parse_query(req->uri, &args);
    reset = evhttp_find_header(&args, "reset");
    if (reset != NULL && strcmp(reset, "1") == 0) {
        total_depth_high_water = 0;
        n_puts = 0;
        n_gets = 0;
    } else {
        format = evhttp_find_header(&args, "format");
        
        if ((format != NULL) && (strcmp(format, "json") == 0)) {
            evbuffer_add_printf(evb, "{");
            evbuffer_add_printf(evb, "\"puts\": %"PRIu64",", n_puts);
            evbuffer_add_printf(evb, "\"gets\": %"PRIu64",", n_gets);
            evbuffer_add_printf(evb, "\"depth\": %"PRIu64",", total_depth);
            evbuffer_add_printf(evb, "\"depth_high_water\": %"PRIu64",", total_depth_high_water);
            evbuffer_add_printf(evb, "\"bytes\": %ld,", n_bytes);
            evbuffer_add_printf(evb, "\"overflow\": %"PRIu64",", n_overflow);
            // for each queue
            evbuffer_add_printf(evb, "\"queues\" : {");
            comma_flag = 0;
            HASH_ITER(hh, named_queues, queue, tmp_queue) {
                if (comma_flag == 0)  {
                    comma_flag = 1;
                } else {
                    evbuffer_add_printf(evb, ",");
                }
                evbuffer_add_printf(evb, "\"%s\": {", queue->name);
                evbuffer_add_printf(evb, "\"puts\": %"PRIu64",", queue->n_puts);
                evbuffer_add_printf(evb, "\"gets\": %"PRIu64",", queue->n_gets);
                evbuffer_add_printf(evb, "\"depth\": %"PRIu64",", queue->depth);
                evbuffer_add_printf(evb, "\"depth_high_water\": %"PRIu64"}", queue->depth_high_water);
            }
            evbuffer_add_printf(evb, "}");
            
            evbuffer_add_printf(evb, "}\n");
        } else {
            evbuffer_add_printf(evb, "puts:%"PRIu64"\n", n_puts);
            evbuffer_add_printf(evb, "gets:%"PRIu64"\n", n_gets);
            evbuffer_add_printf(evb, "depth:%"PRIu64"\n", total_depth);
            evbuffer_add_printf(evb, "depth_high_water:%"PRIu64"\n", total_depth_high_water);
            evbuffer_add_printf(evb, "bytes:%ld\n", n_bytes);
            evbuffer_add_printf(evb, "overflow:%"PRIu64"\n", n_overflow);
            
            evbuffer_add_printf(evb, "\n\n");
            HASH_ITER(hh, named_queues, queue, tmp_queue) {
                evbuffer_add_printf(evb, "%20s gets:%"PRIu64" puts:%"PRIu64" depth:%"PRIu64" depth_high_water:%"PRIu64"\n", queue->name, queue->n_gets, queue->n_puts, queue->depth, queue->depth_high_water);
            }
        }
    }
    
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evhttp_clear_headers(&args);
}

void
get(struct evhttp_request *req, struct evbuffer *evb, void *ctx)
{
    struct evkeyvalq args;
    struct queue_entry *entry;
    struct NamedQueue *queue;
    const char *queue_name;
    
    n_gets++;
    evhttp_parse_query(req->uri, &args);
    queue_name = evhttp_find_header(&args, "queue");
    if (!queue_name) {
        queue_name = "default";
    }
    HASH_FIND_STR(named_queues, queue_name, queue);
    if (!queue) {
        evhttp_send_reply(req, HTTP_OK, "OK", evb);
        evhttp_clear_headers(&args);
        return;
    }
    
    queue->n_gets++;
    entry = TAILQ_FIRST(&(queue->queue_data));
    if (entry != NULL) {
        evbuffer_add_printf(evb, "%s", entry->data);
        TAILQ_REMOVE(&(queue->queue_data), entry, entries);
        free(entry);
        total_depth--;
        queue->depth--;
    }
    
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evhttp_clear_headers(&args);
}

void
put(struct evhttp_request *req, struct evbuffer *evb, void *ctx)
{
    struct evkeyvalq args;
    struct queue_entry *entry;
    const char *data;
    const char *queue_name;
    size_t size;
    struct NamedQueue *named_queue;
    
    n_puts++;
    evhttp_parse_query(req->uri, &args);
    queue_name = evhttp_find_header(&args, "queue");
    if (!queue_name) {
        queue_name = "default";
    }
    data = evhttp_find_header(&args, "data");
    if (data == NULL) {
        evbuffer_add_printf(evb, "%s\n", "missing data");
        evhttp_send_reply(req, HTTP_BADREQUEST, "OK", evb);
        evhttp_clear_headers(&args);
        return;
    }

    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    
    named_queue = get_or_create_queue((char *)queue_name);
    
    size = strlen(data);
    entry = malloc(sizeof(*entry)+size);
    entry->bytes = size;
    strcpy(entry->data, data);
    TAILQ_INSERT_TAIL(&(named_queue->queue_data), entry, entries);
    n_bytes += size;
    total_depth++;
    named_queue->depth++;
    if (total_depth > total_depth_high_water) {
        total_depth_high_water = total_depth;
    }
    if (named_queue->depth > named_queue->depth_high_water) {
        named_queue->depth_high_water = named_queue->depth;
    }
    while ((max_depth > 0 && total_depth > max_depth) 
           || (max_bytes > 0 && n_bytes > max_bytes)) {
        overflow_one(named_queue);
    }
    evhttp_clear_headers(&args);
}

void
dump(struct evhttp_request *req, struct evbuffer *evb, void *ctx)
{
    struct evkeyvalq args;
    struct queue_entry *entry;
    struct NamedQueue *queue;
    const char *queue_name;
    
    evhttp_parse_query(req->uri, &args);
    queue_name = evhttp_find_header(&args, "queue");
    if (!queue_name) {
        queue_name = "default";
    }

    HASH_FIND_STR(named_queues, queue_name, queue);
    if (queue) {
        TAILQ_FOREACH(entry, &(queue->queue_data), entries) {
            evbuffer_add_printf(evb, "%s\n", entry->data);
        }
    }
    
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
}

void usage()
{   
    fprintf(stderr, "%s: A simple http buffer queue.\n", progname);
    fprintf(stderr, "Version %s, http://code.google.com/p/simplehttp/\n", VERSION);
    option_help();
    exit(1);
}   

int version_cb(int value) {
    fprintf(stdout, "Version: %s\n", VERSION);
    return 0;
}

int
main(int argc, char **argv)
{
    struct NamedQueue *queue, *tmp_queue;

    define_simplehttp_options();
    option_define_str("overflow_log", OPT_OPTIONAL, NULL, &overflow_log, NULL, "file to write data beyond --max-depth or --max-bytes");
    // float?
    option_define_int("max_bytes", OPT_OPTIONAL, 0, NULL, NULL, "memory limit");
    option_define_int("max_depth", OPT_OPTIONAL, 0, NULL, NULL, "maximum items in queue");
    option_define_bool("version", OPT_OPTIONAL, 0, NULL, version_cb, VERSION);
    
    if (!option_parse_command_line(argc, argv)){
        return 1;
    }
    max_bytes = (size_t)option_get_int("max_bytes");
    max_depth = (uint64_t)option_get_int("max_depth");

    if (overflow_log) {
        overflow_log_fp = fopen(overflow_log, "a");
        if (!overflow_log_fp) {
            perror("fopen failed: ");
            exit(1);
        }
        fprintf(stdout, "opened --overflow-log: %s\n", overflow_log);
    }

    fprintf(stderr, "Version: %s, http://code.google.com/p/simplehttp/\n", VERSION);
    fprintf(stderr, "use --help for options\n");
    simplehttp_init();
    signal(SIGHUP, hup_handler);
    simplehttp_set_cb("/put*", put, NULL);
    simplehttp_set_cb("/get*", get, NULL);
    simplehttp_set_cb("/dump*", dump, NULL);
    simplehttp_set_cb("/stats*", stats, NULL);
    simplehttp_main();
    free_options();
    
    if (overflow_log_fp) {
        HASH_ITER(hh, named_queues, queue, tmp_queue) {
            while (queue->depth) {
                overflow_one(queue);
            }
            fclose(overflow_log_fp);
        }
    }
    return 0;
}

