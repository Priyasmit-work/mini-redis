#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>

#define MAX_CLIENTS 1000
#define BUFFER_SIZE 4096
#define MAX_KEYS 10000
#define MAX_KEY_LEN 256
#define MAX_VALUE_LEN 1048576

typedef struct {
    char key[MAX_KEY_LEN];
    char *value;
    size_t value_len;
    time_t expire_at;
    int used;
} KeyValue;

typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_len;
} Client;

static KeyValue store[MAX_KEYS];
static Client clients[MAX_CLIENTS];
static int client_count = 0;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL");
    }
}

Client* get_client(int fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd == fd) return &clients[i];
    }
    return NULL;
}

void add_client(int fd) {
    if (client_count >= MAX_CLIENTS) return;
    clients[client_count].fd = fd;
    clients[client_count].buffer_len = 0;
    client_count++;
}

void remove_client(int fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd == fd) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            return;
        }
    }
}

int find_key(const char *key) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_KEYS; i++) {
        if (store[i].used && strcmp(store[i].key, key) == 0) {
            if (store[i].expire_at > 0 && store[i].expire_at < now) {
                free(store[i].value);
                store[i].used = 0;
                return -1;
            }
            return i;
        }
    }
    return -1;
}

int find_free_slot() {
    for (int i = 0; i < MAX_KEYS; i++) {
        if (!store[i].used) return i;
    }
    return -1;
}

void send_response(int fd, const char *response, size_t len) {
    write(fd, response, len);
}

void send_simple_string(int fd, const char *str) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "+%s\r\n", str);
    send_response(fd, response, strlen(response));
}

void send_error(int fd, const char *msg) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "-ERR %s\r\n", msg);
    send_response(fd, response, strlen(response));
}

void send_integer(int fd, long long num) {
    char response[64];
    snprintf(response, sizeof(response), ":%lld\r\n", num);
    send_response(fd, response, strlen(response));
}

void send_bulk_string(int fd, const char *str, size_t len) {
    char response[BUFFER_SIZE];
    if (str == NULL) {
        snprintf(response, sizeof(response), "$-1\r\n");
    } else {
        snprintf(response, sizeof(response), "$%zu\r\n", len);
        send_response(fd, response, strlen(response));
        send_response(fd, str, len);
        send_response(fd, "\r\n", 2);
        return;
    }
    send_response(fd, response, strlen(response));
}

void send_null_bulk_string(int fd) {
    send_response(fd, "$-1\r\n", 5);
}

void handle_ping(int fd, char **args, int argc) {
    if (argc > 0) {
        send_bulk_string(fd, args[0], strlen(args[0]));
    } else {
        send_simple_string(fd, "PONG");
    }
}

void handle_set(int fd, char **args, int argc) {
    if (argc < 2) {
        send_error(fd, "wrong number of arguments for 'set' command");
        return;
    }
    
    const char *key = args[0];
    const char *value = args[1];
    size_t value_len = strlen(value);
    
    int idx = find_key(key);
    if (idx < 0) {
        idx = find_free_slot();
        if (idx < 0) {
            send_error(fd, "out of memory");
            return;
        }
    } else {
        free(store[idx].value);
    }
    
    store[idx].used = 1;
    strncpy(store[idx].key, key, MAX_KEY_LEN - 1);
    store[idx].key[MAX_KEY_LEN - 1] = '\0';
    store[idx].value = malloc(value_len + 1);
    memcpy(store[idx].value, value, value_len + 1);
    store[idx].value_len = value_len;
    store[idx].expire_at = 0;
    
    if (argc >= 4 && strcasecmp(args[2], "EX") == 0) {
        int ttl = atoi(args[3]);
        if (ttl > 0) {
            store[idx].expire_at = time(NULL) + ttl;
        }
    } else if (argc >= 4 && strcasecmp(args[2], "PX") == 0) {
        long long ttl_ms = atoll(args[3]);
        if (ttl_ms > 0) {
            store[idx].expire_at = time(NULL) + (ttl_ms / 1000);
        }
    }
    
    send_simple_string(fd, "OK");
}

void handle_get(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'get' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        send_null_bulk_string(fd);
    } else {
        send_bulk_string(fd, store[idx].value, store[idx].value_len);
    }
}

void handle_del(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'del' command");
        return;
    }
    
    int deleted = 0;
    for (int i = 0; i < argc; i++) {
        int idx = find_key(args[i]);
        if (idx >= 0) {
            free(store[idx].value);
            store[idx].used = 0;
            deleted++;
        }
    }
    send_integer(fd, deleted);
}

void handle_exists(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'exists' command");
        return;
    }
    
    int count = 0;
    for (int i = 0; i < argc; i++) {
        if (find_key(args[i]) >= 0) count++;
    }
    send_integer(fd, count);
}

void handle_incr(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'incr' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        int slot = find_free_slot();
        if (slot < 0) {
            send_error(fd, "out of memory");
            return;
        }
        store[slot].used = 1;
        strncpy(store[slot].key, args[0], MAX_KEY_LEN - 1);
        store[slot].value = malloc(2);
        strcpy(store[slot].value, "1");
        store[slot].value_len = 1;
        store[slot].expire_at = 0;
        send_integer(fd, 1);
    } else {
        long long val = atoll(store[idx].value);
        val++;
        free(store[idx].value);
        store[idx].value = malloc(32);
        snprintf(store[idx].value, 32, "%lld", val);
        store[idx].value_len = strlen(store[idx].value);
        send_integer(fd, val);
    }
}

void handle_decr(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'decr' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        int slot = find_free_slot();
        if (slot < 0) {
            send_error(fd, "out of memory");
            return;
        }
        store[slot].used = 1;
        strncpy(store[slot].key, args[0], MAX_KEY_LEN - 1);
        store[slot].value = malloc(3);
        strcpy(store[slot].value, "-1");
        store[slot].value_len = 2;
        store[slot].expire_at = 0;
        send_integer(fd, -1);
    } else {
        long long val = atoll(store[idx].value);
        val--;
        free(store[idx].value);
        store[idx].value = malloc(32);
        snprintf(store[idx].value, 32, "%lld", val);
        store[idx].value_len = strlen(store[idx].value);
        send_integer(fd, val);
    }
}

void handle_ttl(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'ttl' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        send_integer(fd, -2);
    } else if (store[idx].expire_at == 0) {
        send_integer(fd, -1);
    } else {
        time_t remaining = store[idx].expire_at - time(NULL);
        send_integer(fd, remaining > 0 ? remaining : -2);
    }
}

void handle_expire(int fd, char **args, int argc) {
    if (argc < 2) {
        send_error(fd, "wrong number of arguments for 'expire' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        send_integer(fd, 0);
    } else {
        int seconds = atoi(args[1]);
        store[idx].expire_at = time(NULL) + seconds;
        send_integer(fd, 1);
    }
}

void handle_dbsize(int fd) {
    int count = 0;
    time_t now = time(NULL);
    for (int i = 0; i < MAX_KEYS; i++) {
        if (store[i].used) {
            if (store[i].expire_at > 0 && store[i].expire_at < now) {
                free(store[i].value);
                store[i].used = 0;
            } else {
                count++;
            }
        }
    }
    send_integer(fd, count);
}

void handle_flushall(int fd) {
    for (int i = 0; i < MAX_KEYS; i++) {
        if (store[i].used) {
            free(store[i].value);
            store[i].used = 0;
        }
    }
    send_simple_string(fd, "OK");
}

void handle_append(int fd, char **args, int argc) {
    if (argc < 2) {
        send_error(fd, "wrong number of arguments for 'append' command");
        return;
    }
    
    int idx = find_key(args[0]);
    size_t append_len = strlen(args[1]);
    
    if (idx < 0) {
        handle_set(fd, args, argc);
    } else {
        size_t new_len = store[idx].value_len + append_len;
        store[idx].value = realloc(store[idx].value, new_len + 1);
        memcpy(store[idx].value + store[idx].value_len, args[1], append_len + 1);
        store[idx].value_len = new_len;
        send_integer(fd, new_len);
    }
}

void handle_strlen(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'strlen' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        send_integer(fd, 0);
    } else {
        send_integer(fd, store[idx].value_len);
    }
}

void handle_type(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'type' command");
        return;
    }
    
    int idx = find_key(args[0]);
    if (idx < 0) {
        send_simple_string(fd, "none");
    } else {
        send_simple_string(fd, "string");
    }
}

void handle_select(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'select' command");
        return;
    }
    send_simple_string(fd, "OK");
}

void handle_echo(int fd, char **args, int argc) {
    if (argc < 1) {
        send_error(fd, "wrong number of arguments for 'echo' command");
        return;
    }
    send_bulk_string(fd, args[0], strlen(args[0]));
}

int parse_resp(char *buffer, size_t len, char **cmd, char **args, int *argc) {
    if (len < 3) return -1;
    
    if (buffer[0] != '*') return -1;
    
    int count = atoi(buffer + 1);
    if (count <= 0 || count > 20) return -1;
    
    char *ptr = buffer;
    int arg_idx = 0;
    
    for (int i = 0; i < count; i++) {
        ptr = strstr(ptr, "\r\n");
        if (!ptr) return -1;
        ptr += 2;
        
        if (ptr[0] != '$') return -1;
        
        int str_len = atoi(ptr + 1);
        if (str_len < 0 || str_len > MAX_VALUE_LEN) return -1;
        
        ptr = strstr(ptr, "\r\n");
        if (!ptr) return -1;
        ptr += 2;
        
        if (i == 0) {
            *cmd = ptr;
        } else {
            args[arg_idx++] = ptr;
        }
        
        if (ptr[str_len] != '\r' || ptr[str_len + 1] != '\n') {
            if (ptr + str_len + 2 <= buffer + len) {
                ptr[str_len] = '\0';
            }
        }
        ptr[str_len] = '\0';
    }
    
    *argc = arg_idx;
    return 0;
}

void handle_command(int fd, char *buffer, size_t len) {
    char *cmd = NULL;
    char *args[20];
    int argc = 0;
    
    if (parse_resp(buffer, len, &cmd, args, &argc) != 0) {
        if (strncasecmp(buffer, "PING", 4) == 0) {
            send_simple_string(fd, "PONG");
        } else {
            send_error(fd, "invalid RESP protocol");
        }
        return;
    }
    
    for (char *p = cmd; *p; p++) {
        *p = toupper(*p);
    }
    
    if (strcmp(cmd, "PING") == 0) {
        handle_ping(fd, args, argc);
    } else if (strcmp(cmd, "SET") == 0) {
        handle_set(fd, args, argc);
    } else if (strcmp(cmd, "GET") == 0) {
        handle_get(fd, args, argc);
    } else if (strcmp(cmd, "DEL") == 0) {
        handle_del(fd, args, argc);
    } else if (strcmp(cmd, "EXISTS") == 0) {
        handle_exists(fd, args, argc);
    } else if (strcmp(cmd, "INCR") == 0) {
        handle_incr(fd, args, argc);
    } else if (strcmp(cmd, "DECR") == 0) {
        handle_decr(fd, args, argc);
    } else if (strcmp(cmd, "TTL") == 0) {
        handle_ttl(fd, args, argc);
    } else if (strcmp(cmd, "EXPIRE") == 0) {
        handle_expire(fd, args, argc);
    } else if (strcmp(cmd, "DBSIZE") == 0) {
        handle_dbsize(fd);
    } else if (strcmp(cmd, "FLUSHALL") == 0) {
        handle_flushall(fd);
    } else if (strcmp(cmd, "APPEND") == 0) {
        handle_append(fd, args, argc);
    } else if (strcmp(cmd, "STRLEN") == 0) {
        handle_strlen(fd, args, argc);
    } else if (strcmp(cmd, "TYPE") == 0) {
        handle_type(fd, args, argc);
    } else if (strcmp(cmd, "SELECT") == 0) {
        handle_select(fd, args, argc);
    } else if (strcmp(cmd, "ECHO") == 0) {
        handle_echo(fd, args, argc);
    } else if (strcmp(cmd, "COMMAND") == 0) {
        send_simple_string(fd, "OK");
    } else if (strcmp(cmd, "INFO") == 0) {
        send_bulk_string(fd, "# Server\nredis_version:0.1.0\n", 26);
    } else if (strcmp(cmd, "QUIT") == 0) {
        send_simple_string(fd, "OK");
        close(fd);
        remove_client(fd);
    } else {
        char err[128];
        snprintf(err, sizeof(err), "unknown command '%s'", cmd);
        send_error(fd, err);
    }
}

int main() {
    memset(store, 0, sizeof(store));
    memset(clients, 0, sizeof(clients));
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(6379);
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    set_nonblocking(server_fd);
    printf("Redis clone listening on port 6379...\n");
    
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
        return 1;
    }
    
    struct epoll_event events[MAX_CLIENTS + 1];
    
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_CLIENTS + 1, -1);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }
                
                set_nonblocking(client_fd);
                add_client(client_fd);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                
                printf("Client connected: fd=%d\n", client_fd);
            } else {
                int client_fd = events[i].data.fd;
                Client *client = get_client(client_fd);
                
                if (!client) continue;
                
                char temp_buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(client_fd, temp_buffer, sizeof(temp_buffer) - 1);
                
                if (bytes_read <= 0) {
                    printf("Client disconnected: fd=%d\n", client_fd);
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    remove_client(client_fd);
                } else {
                    temp_buffer[bytes_read] = '\0';
                    
                    size_t copy_len = bytes_read;
                    if (client->buffer_len + copy_len >= BUFFER_SIZE) {
                        copy_len = BUFFER_SIZE - client->buffer_len - 1;
                    }
                    
                    if (copy_len > 0) {
                        memcpy(client->buffer + client->buffer_len, temp_buffer, copy_len);
                        client->buffer_len += copy_len;
                        client->buffer[client->buffer_len] = '\0';
                    }
                    
                    handle_command(client_fd, client->buffer, client->buffer_len);
                    client->buffer_len = 0;
                }
            }
        }
    }
    
    close(epoll_fd);
    close(server_fd);
    
    for (int i = 0; i < MAX_KEYS; i++) {
        if (store[i].used && store[i].value) {
            free(store[i].value);
        }
    }
    
    return 0;
}
