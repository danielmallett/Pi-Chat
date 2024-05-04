/* CS360 Lab 9 - Threaded Chat Server 
 * Daniel Mallett - dmallett@vols.utk.edu 
 * 05/04/2024
 * chat-server.c - a multi-threaded chat server
 * that allows clients to connect to various chat
 * rooms and communicate over sockets. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "sockettome.h"
#include "dllist.h"
#include "jrb.h"

/* pthread wrappers */
int my_pthread_join(pthread_t thread)
{
	int ret;
	void *status;
	ret = pthread_join(thread, &status);
	if (ret != 0) {
		perror("pthread_join"); 
		exit(1);
	}
}

pthread_cond_t *new_cond()
{
	pthread_cond_t *cond;

	cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
	if (cond == NULL) { perror("malloc pthread_cond_t"); exit(1); }
	pthread_cond_init(cond, NULL);

	return cond;
}

pthread_mutex_t *new_mutex()
{
	pthread_mutex_t *mutex;
	
	mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	if (mutex == NULL) { perror("malloc pthread_mutex_t"); exit(1); }
	pthread_mutex_init(mutex, NULL);
	return mutex;
}

int my_mutex_lock(pthread_mutex_t *lock)
{
	int ret;
	if ((ret = pthread_mutex_lock(lock)) != 0) { 
		perror("pthread_mutex_lock"); 
		exit(1); 
	}
	return ret;
}

int my_mutex_unlock(pthread_mutex_t *lock)
{
	int ret;
	if ((ret = pthread_mutex_unlock(lock)) != 0) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
	return ret;
}

int my_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int ret;
	if ((ret = pthread_cond_wait(cond, mutex)) != 0) {
		perror("pthread_cond_wait");
		exit(1);
	}
	return ret;
}

int my_cond_signal(pthread_cond_t *cond)
{
	int ret;
	if ((ret = pthread_cond_signal(cond)) != 0) {
		perror("pthread_cond_signal");
		exit(1);
	}
	return ret;
}

/* helper, removes ending newline, returns the new length of the string */
int remove_newline(char *str, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (str[i] == '\n') {
			str[i] = '\0';
			return i;
		}
	}
	return len;
}

/* structs for server, rooms, and client data */
typedef struct server_info {
	int port;			/* port number greater than 8000 */
	int socket;			/* open socket */
	JRB chat_rooms;		/* tree of chatroom structs */
} server;

typedef struct room_info {
	char *name;				/* chatroom name */
	Dllist clients;			/* list of connected clients */
	Dllist messages;		/* list of inputs from clients */
	pthread_mutex_t *lock;
	pthread_cond_t *cond;	
	pthread_t tid;			
} room;

typedef struct client_info {
	FILE *socket_io[2];		/* 0 for input, 1 for output */
	server *server;			/* ptr to the server info */
	char *name;				/* chat-name entered by clients */
	room *room;				/* ptr to the chatroom they're connected to */
	Dllist client_node;		/* ptr to node in room list for fast disconnect */
	pthread_t tid;			
} client;

/* allocate and initialize a new client */
client *new_client(server *s, int fd)
{
	client *c;

	c = (client *) malloc(sizeof(client));
	c->server = s;
	c->socket_io[0] = fdopen(fd, "r");
	c->socket_io[1] = fdopen(fd, "w");
	c->name = NULL;
	c->room = NULL;

	return c;
}

/* allocate and initialize a new chatroom */
room *new_room(char *name) 
{
	room *r;

	r = (room *) malloc(sizeof(room));
	r->name = name; 
	r->clients = new_dllist();
	r->messages = new_dllist();
	r->lock = new_mutex();
	r->cond = new_cond();

	return r;
}

/* lock chatroom mutex and push a new message onto the queue */
void send_message(client *c, char *message)
{
	my_mutex_lock(c->room->lock);
	dll_append(c->room->messages, new_jval_s(strdup(message)));
	my_cond_signal(c->room->cond);
	my_mutex_unlock(c->room->lock);
}

/* clean up after the client disconnects */
void close_connection(client *c)
{
	char *message;
	message = (char *) malloc(sizeof(char) * (strlen(c->name) + 11));
	strcpy(message, c->name);
	strcat(message, " has left\n");

	fclose(c->socket_io[0]);
	fclose(c->socket_io[1]);

	send_message(c, message);
	free(message);

	my_mutex_lock(c->room->lock);
	dll_delete_node(c->client_node);
	my_mutex_unlock(c->room->lock);

	free(c->name);	
	free(c);
}

/* thread for each chat room */
void *room_thread(void *room_info)
{
	client *c;
	room *r;
	char *message;
	Dllist client_node;
	Dllist message_node;

	r = (room *) room_info;

	my_mutex_lock(r->lock);
	while (1) {
		while (!dll_empty(r->messages)) {
			message_node = dll_first(r->messages);
			message = message_node->val.s;

			/* send message to all clients in chatroom */
			dll_traverse(client_node, r->clients) {
				c = (client *) client_node->val.v;
				fputs(message, c->socket_io[1]);
				fflush(c->socket_io[1]);
			}

			free(message_node->val.s);
			dll_delete_node(message_node);
		}

		/* wait for client message signal */
		my_cond_wait(r->cond, r->lock);
	}

	return NULL;
}

/* thread for each client */
void *process_connection(void *client_info)
{
	client *c, *other_client;
	char buf[1000];
	char *name, *room_name;
	char *message;
	char *ret;
	JRB room_node;
	room *r;
	Dllist tmp;
	int len;

	c = (client *) client_info;

	/* print server info */
	fputs("Chat Rooms:\n\n", c->socket_io[1]);
	jrb_traverse(room_node, c->server->chat_rooms) {
		r = (room *) room_node->val.v;

		sprintf(buf, "%s:", r->name);
		fputs(buf, c->socket_io[1]);

		dll_traverse(tmp, r->clients) {
			other_client = (client *) tmp->val.v;
			sprintf(buf, " %s", other_client->name);
			fputs(buf, c->socket_io[1]);
		}
		fputs("\n", c->socket_io[1]);
	}

	/* get client chat name */
	fputs("\nEnter your chat name (no spaces):\n", c->socket_io[1]);
	fflush(c->socket_io[1]);

	ret = fgets(buf, 1000, c->socket_io[0]);
	if (ret == NULL) {
		fclose(c->socket_io[0]);
		fclose(c->socket_io[1]);
		return NULL;
	}

	len = strlen(buf);
	len = remove_newline(buf, len);
	name = (char *) malloc(sizeof(char) * len + 1);
	strcpy(name, buf);
	c->name = name;

	/* connect client to chatroom */
	while (c->room == NULL) {
		fputs("Enter chat room:\n", c->socket_io[1]);
		fflush(c->socket_io[1]);

		ret = fgets(buf, 1000, c->socket_io[0]);
		if (ret == NULL) {
			fclose(c->socket_io[0]);
			fclose(c->socket_io[1]);
			return NULL;
		}

		len = strlen(buf);
		len = remove_newline(buf, len);
		room_name = (char *) malloc(sizeof(char) * len + 1);
		strcpy(room_name, buf);

		room_node = jrb_find_str(c->server->chat_rooms, room_name);
		if (room_node == NULL) {
			sprintf(buf, "No chat room %s\n", room_name);
			fputs(buf, c->socket_io[1]);
			fflush(c->socket_io[1]);
		} else {
			r = (room *) room_node->val.v;
			c->room = r;
			my_mutex_lock(r->lock);
			dll_append(r->clients, new_jval_v((void *) c));
			c->client_node = dll_last(r->clients);
			my_mutex_unlock(r->lock);
		}
	}

	/* send join message */
	len = strlen(c->name);
	message = (char *) malloc(sizeof(char) * len + 13); 
	strcpy(message, c->name);
	strcat(message, " has joined\n");

	send_message(c, message);
	free(message);

	/* send messages */
	while (fgets(buf, 1000, c->socket_io[0]) != NULL) {
		len = strlen(buf);
		len += strlen(c->name);
		message = (char *) malloc(sizeof(char) * (len + 3)); 
		strcpy(message, c->name);
		strcat(message, ": ");
		strcat(message, buf);

		send_message(c, message);
		free(message);
	}

	close_connection(c);
	return NULL;
}

/* allocate and initialize server-info struct */
server *new_server(int port, int num_rooms, char **chat_rooms)
{
	server *s;
	room *r;
	FILE *socket_in, *socket_out;
	int i;

	s = (server *) malloc(sizeof(server));
	s->port = port;
	s->chat_rooms = make_jrb();

	/* init room_info stucts and tree of room_infos */
	for (i = 0; i < num_rooms; i++) {
		r = new_room(chat_rooms[i]);
		jrb_insert_str(s->chat_rooms, strdup(chat_rooms[i]), new_jval_v((void *) r));
	}

	s->socket = serve_socket(port);

	return s;
}

/* wait for clients to attach to socket and create new a new client thread */
void run_server(server *s)
{
	client *c;
	pthread_t tid;
	JRB room_node;
	int fd;

	/* fork room threads */
	jrb_traverse(room_node, s->chat_rooms) {
		if (pthread_create(&tid, NULL, room_thread, room_node->val.v) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}

	/* handle client connections */
	while (1) {
		fd = accept_connection(s->socket);
		c = new_client(s, fd);

		if (pthread_create(&(c->tid), NULL, process_connection, (void *) c) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}
}

/* clean up server memory */
void delete_server(server *s)
{
	JRB room_node;
	Dllist tmp_dll;
	room *r;
	client *c;

	jrb_traverse(room_node, s->chat_rooms) {
		r = (room *) room_node->val.v;

		dll_traverse(tmp_dll, r->clients) {
			c = (client *) tmp_dll->val.v;
			fclose(c->socket_io[0]);
			fclose(c->socket_io[1]);
			free(c->name);
			free(c);
		}
		free_dllist(r->clients);

		dll_traverse(tmp_dll, r->messages) {
			free(tmp_dll->val.s);
		}
		free_dllist(r->messages);

		free(r->lock);
		free(r->cond);
		free(r->name);
		free(r);
	}

	jrb_free_tree(s->chat_rooms);
	free(s);
}

int main(int argc, char **argv)
{
	int port, num_rooms;
	server *s;

	if (argc < 3) {
		fprintf(stderr, "usage: ./chat-server port Chat-Room-Names ...\n");
		exit(1);
	}
	num_rooms = argc - 2;

	/* port must be > 8000 */
	if (sscanf(argv[1], "%d", &port) != 1) { fprintf(stderr, "bad port\n"); exit(1); }
	if (port < 8000) { fprintf(stderr, "port must be > 8000\n"); exit(1); }

	s = new_server(port, num_rooms, argv + 2);
	run_server(s);

	delete_server(s);
	return 0;
}
