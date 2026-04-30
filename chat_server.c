#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sock_util.h"
#include <stdbool.h>

#define SERV_PORT 8999
#define MAXMSG    512
#define MAXCLIENTS 100

typedef struct client_info {
    char username[16];
    struct sockaddr_in addr;
} client_info;

client_info clients[MAXCLIENTS];
int client_count = 0;

/* Vorwärtsdeklaration */
void handleChat(int sockfd, struct sockaddr *cliaddr, socklen_t clilen);

int main(void)
{
    int sockfd;
    struct sockaddr_in srvaddr, cliaddr;

    /* 1. Socket anlegen (UDP = SOCK_DGRAM) */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* 2. Server-Adresse befüllen */
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family      = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY); // alle Interfaces
    srvaddr.sin_port        = htons(SERV_PORT);

    /* 3. Adresse an Socket binden */
    if (bind(sockfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0)
        err_abort("Cannot bind socket-address");

    /* 4. Chat-Schleife starten */
    handleChat(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

    return 0;
}

bool is_username_valid(const char* name) {
    if (strlen(name) == 0 || strlen(name) > 15) {
        return false; // Username zu lang oder leer
    }


    for (size_t i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].username, name) == 0) {
            return false; // Username bereits vergeben
        }
    }
    
    return true;
}

char* is_client_registered(struct sockaddr *cliaddr) {
    for (size_t i = 0; i < client_count; i++)
    {
        if (memcmp(&clients[i].addr, cliaddr, sizeof(struct sockaddr_in)) == 0) {
            return clients[i].username; // Client gefunden
        }
    }
    return NULL; // Client nicht gefunden
}

void send_good_response(int sockfd, struct sockaddr *cliaddr, socklen_t clilen) {
    char msg[] = "/ok\r\n";
    size_t msg_len = strlen(msg);
    if (sendto(sockfd, msg, msg_len, 0, cliaddr, clilen) != (ssize_t)msg_len) {
        err_abort("sendto() fails");
    }
}

void send_error_response(int sockfd, struct sockaddr *cliaddr, socklen_t clilen) {
    char msg[] = "/error\r\n";
    size_t msg_len = strlen(msg);
    if (sendto(sockfd, msg, msg_len, 0, cliaddr, clilen) != (ssize_t)msg_len) {
        err_abort("sendto() fails");
    }
}

bool handle_nick(int sockfd, struct sockaddr *cliaddr, socklen_t clilen, char* command_arg) {
    if (command_arg == NULL) {
        return false;
    }

    if (!is_username_valid(command_arg)) {
        return false;
    }

    int i;
    for (i = 0; i < client_count; i++) {
        if (memcmp(&clients[i].addr, cliaddr, sizeof(struct sockaddr_in)) == 0) {
            break; // Client bereits bekannt
        }
    }

    if (i == client_count && client_count < MAXCLIENTS) {
        // Neuer Client, Informationen speichern
        clients[client_count].addr = *(struct sockaddr_in *)cliaddr;
        strncpy(clients[client_count].username, command_arg, sizeof(clients[client_count].username) - 1);
        clients[client_count].username[sizeof(clients[client_count].username) - 1] = '\0'; // Nullterminierung sicherstellen
        client_count++;
    } else {
        // Client bereits bekannt, nur Username aktualisieren
        strncpy(clients[i].username, command_arg, sizeof(clients[i].username) - 1);
        clients[i].username[sizeof(clients[i].username) - 1] = '\0'; // Nullterminierung sicherstellen
    }

    return true;
}

bool handle_names(int sockfd, struct sockaddr *cliaddr, socklen_t clilen) {
    /*Sendet eine Liste aller Verbundenen Clients zurück*/
    char msg[MAXMSG];
    size_t offset = 0;

    for (size_t i = 0; i < client_count; i++)
    {
        int written = snprintf(msg + offset, sizeof(msg) - offset, "%s\r\n", clients[i].username);
        if (written < 0 || (size_t)written >= sizeof(msg) - offset) {
            break; // Keine weitere Nachricht mehr hinzufügen, wenn der Puffer voll ist
        }
        offset += written;
    }

    if (sendto(sockfd, msg, offset, 0, cliaddr, clilen) != (ssize_t)offset) {
        err_abort("sendto() fails");
    }
    
    return true;
}

void handleCommand(int sockfd, struct sockaddr *cliaddr, socklen_t clilen, char* command) {
    
    /* NewLine entfernen */
    size_t cmd_len = strlen(command);
    if (cmd_len > 0 && (command[cmd_len-1] == '\n' || command[cmd_len-1] == '\r')) {
        command[cmd_len-1] = '\0';
    }
    if (cmd_len > 1 && command[cmd_len-2] == '\r') {
        command[cmd_len-2] = '\0';
    }

    /*Command in Name und Argumente trennen*/
    char* command_name = strtok(command, " ");
    char* command_arg = strtok(NULL, " ");

    bool return_val = false; // Rückgabewert der Befehlsverarbeitung, true = Erfolg, false = Fehler

    if (strcmp(command_name, "/nick") == 0) {
        return_val = handle_nick(sockfd, cliaddr, clilen, command_arg);
    } else if (strcmp(command_name, "/names") == 0) {
        return_val = handle_names(sockfd, cliaddr, clilen);
    } else if (strcmp(command_name, "/bye") == 0) {
        /*Client möchte sich abmelden, entferne ihn aus der Liste der Clients*/

        for (size_t i = 0; i < client_count; i++)
        {
            if (memcmp(&clients[i].addr, cliaddr, sizeof(struct sockaddr_in)) == 0) {
                // Client gefunden, entferne ihn durch Überschreiben mit dem letzten Eintrag
                clients[i] = clients[client_count - 1];
                client_count--;
                return_val = true;
                break;
            }
        }

        return; // Keine Rückmeldung an den Client, da er sich abmeldet
    }

    if(return_val) {
        send_good_response(sockfd, cliaddr, clilen);
    } else {
        send_error_response(sockfd, cliaddr, clilen);
    }
}

void handleChat(int sockfd, struct sockaddr *cliaddr, socklen_t clilen)
{
    ssize_t   n;
    socklen_t len;
    char      msg[MAXMSG];
    char      msg2[MAXMSG + 20];

    for (;;) {
        len = clilen;

        /* Blockiert, bis ein Datagramm ankommt */
        n = recvfrom(sockfd, msg, MAXMSG, 0, cliaddr, &len);
        if (n < 0)
            err_abort("recvfrom() fails");

        /* NewLine entfernen */
        if (n > 0 && (msg[n-1] == '\n' || msg[n-1] == '\r')) {
            msg[n-1] = '\0';
            n--;
        }
        if (n > 0 && msg[n-1] == '\r') {
            msg[n-1] = '\0';
            n--;
        }

        /* Ueberpruefe ob Nachricht ein Command ist */
        if (msg[0] == '/') {
            handleCommand(sockfd, cliaddr, clilen, msg);
            continue; // Nach der Bearbeitung des Befehls nicht weiterverarbeiten
        }

        // UserName von Sender finden
        // Null wenn Sender nicht regestriert
        char* sender_username = is_client_registered(cliaddr);
        
        // Nachricht von unbekanntem Client verwerfen
        if (sender_username == NULL) {
            continue;
        }

        /* Antwort zusammenbauen und zurückschicken an alle Verbundenen Clients*/
        sprintf(msg2, "[%s] %s\r\n", sender_username, msg);
        size_t msg_len = strlen(msg2);
        for (size_t j = 0; j < client_count; j++)
        {
            if(memcmp(&clients[j].addr, cliaddr, sizeof(struct sockaddr_in)) == 0) {
                continue; // Nicht an den Sender zurückschicken
            }

            if (sendto(sockfd, msg2, msg_len, 0, (struct sockaddr *)&clients[j].addr, sizeof(clients[j].addr)) != (ssize_t)msg_len) {
                char err_msg[100];
                snprintf(err_msg, sizeof(err_msg), "sendto() fails for client %s", clients[j].username);
                err_abort(err_msg);
            }
        }
    }
}
