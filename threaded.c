// http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver
// http://stackoverflow.com/a/5124450/1027966
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "3000" // The port users will connect to
#define BACKLOG 128 // how many pending connections queue will hold
#define NUM_THREADS 8

// get sockaddr, IPv4 or IPv6
void* get_in_addr (struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*) sa)->sin_addr);
  } else {
    return &(((struct sockaddr_in6*) sa)->sin6_addr);
  }
}

struct ThreadData {
  int sockfd;
  char* body;
};

void* req_handler (void* td) {
  int new_fd, id = (int) pthread_self();
  printf("Thread #%d running\n", id);
  struct ThreadData* data = (struct ThreadData*) td;
  struct sockaddr_storage their_addr; // connector's address
  socklen_t sin_size = sizeof their_addr;
  char s [INET6_ADDRSTRLEN], buf [200];
  const char* const msg = data->body;
  const char* const fmt_header =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: %ld\r\n"
    "\r\n"
    "%s";

  while (1) {
    new_fd = accept(data->sockfd, (struct sockaddr*) &their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);
    printf("Thread #%d: got connection from %s\n", id, s);

    snprintf(buf, 200, fmt_header, strlen(msg), msg);
    if (send(new_fd, buf, strlen(buf), 0) == -1) {
      perror("send");
    }
    close(new_fd);
  }

  return NULL;
}

int main () {
  int sockfd; // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  int yes = 1;
  int rv;

  hints = (struct addrinfo) {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE // use my IP
  };

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1) {
      perror("setsockopt");
      return 1;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server:bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  freeaddrinfo(servinfo); // all done with this struct

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    return 1;
  }

  puts("server: waiting for connections...");

  pthread_t threads [NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; ++i) {
    pthread_create(&threads[i], NULL, req_handler, &(struct ThreadData) {
      .sockfd = sockfd,
      .body = "hello world\n"
    });
  }

  for (int i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }

  close(sockfd);
}

