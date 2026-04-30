#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// Library zur Arbeit mit der Kommandozeile -> IO von Nachrichten nicht blockierend
#include <readline/readline.h>
#include <readline/history.h>


#include "sock_util.h"

#define SERV_PORT 8999
#define MAXMSG    512

int sockfd_global;  // Global variable for use in callbacks
struct sockaddr_in srvaddr_global;  // Globale Serveradresse
socklen_t srvlen_global;  // Globale Laenge der Serveradresse

void dg_cli(int sockfd, struct sockaddr *srvaddr, socklen_t srvlen);

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in srvaddr;

    if (argc != 2)
        err_abort("usage: echo_client <ipaddress>\n");

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        err_abort("socket() failed");

    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family      = AF_INET;
    srvaddr.sin_addr.s_addr = inet_addr(argv[1]);
    srvaddr.sin_port        = htons(SERV_PORT);

    dg_cli(sockfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr));

    close(sockfd);
    return 0;
}

void process_input_line(char *line)
{
    if (line == NULL) {
        // EOF
        exit(0);
    }
    
    // Line in History laden
    add_history(line);
    
    char sendline[MAXMSG];
    strncpy(sendline, line, MAXMSG - 1);
    sendline[MAXMSG - 1] = '\0';
    free(line);
    
    size_t n = strlen(sendline);
    
    /* Nachrichtenende Sicherstellen - \r\n */
    if (n > 0 && sendline[n-1] == '\n') {
        sendline[n-1] = '\r';
        sendline[n] = '\n';
        sendline[n+1] = '\0';
        n = n + 1;
    } else if (n > 0 && sendline[n-1] != '\r') {
        sendline[n] = '\r';
        sendline[n+1] = '\n';
        sendline[n+2] = '\0';
        n = n + 2;
    }
    
    struct sockaddr_in srvaddr;
    memset(&srvaddr, 0, sizeof(srvaddr));
    // Note: address is set up in dg_cli
    
    if (sendto(sockfd_global, sendline, n, 0, (struct sockaddr *)&srvaddr_global, srvlen_global) < 0)
        err_abort("sendto() failed");
}

void dg_cli(int sockfd, struct sockaddr *srvaddr, socklen_t srvlen)
{
    fd_set readfds;
    char recvline[MAXMSG + 20];
    int maxfdp1 = sockfd + 1;
    
    sockfd_global = sockfd;
    srvaddr_global = *(struct sockaddr_in *)srvaddr;
    srvlen_global = srvlen;

    // Readline Callback installieren
    rl_callback_handler_install("> ", process_input_line);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        if (select(maxfdp1, &readfds, NULL, NULL, NULL) < 0)
            err_abort("select() failed");

        if (FD_ISSET(sockfd, &readfds)) {
            ssize_t n = recvfrom(sockfd, recvline, MAXMSG + 20, 0, NULL, NULL);
            if (n < 0)
                err_abort("recvfrom() failed");

            recvline[n] = '\0';
            
            // Promt erneut ausgeben -> Nachricht über der aktuellen Eingabe anzeigen
            printf("\r\n%s", recvline);
            if (recvline[n-1] != '\n')
                printf("\n");
            fflush(stdout);
            
            // Eingabeaufforderung und aktuelle Eingabe erneut anzeigen
            rl_forced_update_display();
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            rl_callback_read_char();
        }
    }
    
    rl_callback_handler_remove(); // Cleanup Readline callback handler
}
