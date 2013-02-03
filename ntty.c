#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

pthread_mutex_t client_mutex;
pthread_cond_t client_exists;

struct client_entry {
	int sock;
	struct client_entry *next;
};

struct client_entry *client_list_head;

void* reader_thread(void* unused)
{
	struct client_entry *entry, *last, *tofree;
	char *linebuf;
	size_t bytes_allocated;
	ssize_t bytes_read;
	int ret;
	int consumed;

	// Main client loop
	consumed = 0;
	linebuf = NULL;
	for (;;) {
		pthread_mutex_lock(&client_mutex);

		// Check if any clients exist
		if (client_list_head) {
			// Check if data is still pending
			if (linebuf == NULL) {
				// Read a line from stdin
				bytes_read = getline(&linebuf, &bytes_allocated, stdin);
				if (bytes_read == -1) {
					pthread_mutex_unlock(&client_mutex);
					break;
				}

				// Data has not been consumed yet
				consumed = 0;
			}

			// Send it to all connected clients
			entry = client_list_head;
			last = NULL;
			while (entry) {
				ret = send(entry->sock, linebuf, bytes_read, 0);
				if (ret == -1) {
					perror("client disconnected");

					// Close the socket descriptor
					close(entry->sock);

					// Remove the client from the list
					if (last != NULL) {
						// Unlink and save the pointer
						last->next = entry->next;
						tofree = entry;
					} else {
						// We're removing the head entry
						client_list_head = entry->next;
						tofree = entry;
					}

					// Skip to the next entry before freeing
					entry = entry->next;

					// Free the old entry
					free(tofree);
				} else {
					// Send was fine, just continue to next entry
					last = entry;
					entry = entry->next;

					// Data was consumed by at least one client
					consumed = 1;
				}
			}

			// If data was consumed, free the line buffer
			if (consumed) {
				free(linebuf);
				linebuf = NULL;
			}
		} else {
			// Wait for new clients
			pthread_cond_wait(&client_exists, &client_mutex);
		}

		pthread_mutex_unlock(&client_mutex);
	}

	// Exit the thread
	pthread_exit(NULL);
}
	

int main(int argc, char *argv[])
{
	int listener;
	int ret;
	short port;
	struct sockaddr_in bindaddr;
	struct client_entry *new_entry;
	pthread_t client_thread;

	// Check for enough arguments
	if (argc < 2) {
		printf("Usage: ntty <port>\n");
		exit(EXIT_FAILURE);
	}

	// Check for valid arguments
	port = atoi(argv[1]);
	if (port == 0) {
		printf("Illegal port number specified\n");
		exit(EXIT_FAILURE);
	}

	// Create the server listener socket
	listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Bind it to the requested port
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_addr.s_addr = 0;
	bindaddr.sin_port = htons(port);
	ret = bind(listener, (struct sockaddr*)&bindaddr, sizeof(bindaddr));
	if (ret == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	// Start listening
	ret = listen(listener, 10);
	if (ret == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Initialize shared variables
	pthread_mutex_init(&client_mutex, NULL);
	pthread_cond_init(&client_exists, NULL);
	client_list_head = NULL;

	// Create the client reader thread
	ret = pthread_create(&client_thread, NULL, reader_thread, NULL);
	if (ret == -1) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}

	printf("Listening on port: %d\n", port);

	// Main server loop
	new_entry = NULL;
	for (;;) {
		struct sockaddr claddr;
		socklen_t claddr_len;

		if (new_entry == NULL) {
			new_entry = (struct client_entry *)malloc(sizeof(*new_entry));
			if (new_entry == NULL)
				continue;
		}

		// Accept a client
		claddr_len = sizeof(claddr);
		new_entry->sock = accept(listener, &claddr, &claddr_len);
		if (new_entry->sock == -1) {
			perror("accept");
			continue;
		}

		printf("Client accepted\n");

		pthread_mutex_lock(&client_mutex);

		// Link it to the head of the list
		new_entry->next = client_list_head;
		client_list_head = new_entry;

		// Signal the condition
		pthread_cond_signal(&client_exists);

		pthread_mutex_unlock(&client_mutex);

		new_entry = NULL;
	}
}
