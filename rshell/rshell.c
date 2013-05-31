#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#include <pty.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define MAX_COUNT_OF_CONNECTIONS 5

int my_write(int fd, const char *str, int len)
{
    int written = 0;
    int write_ret = 0;
    while (written < len)
    {
        write_ret = write(fd, str + written, len - written);
        if (write_ret < 0)
        {
            return -1;
        }
        written += write_ret; 
    }
    return 0;
}

int main()
{
    int main_pid = fork();
    if (main_pid)
    {
        wait();
    }
    else
    {
        setsid();

        struct addrinfo hints; 
        struct addrinfo *result;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        int res_getaddrinfo = getaddrinfo(NULL, "8822", &hints, &result);
        if (res_getaddrinfo != 0)
        {
            const char *error = gai_strerror(res_getaddrinfo);
            my_write(STDERR, error, strlen(error)); 
            exit(EXIT_FAILURE);
        }

        struct addrinfo *it; 
        it = result;
        int sfd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);

        int opt_val = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int));
        
        if (bind(sfd, it->ai_addr, it->ai_addrlen) == -1)
        {
            const char *error = "Coudln't bind\n";
            my_write(STDERR, error, strlen(error));
            exit(EXIT_FAILURE);
        }

        freeaddrinfo(result);
        
        int listen_res = listen(sfd, MAX_COUNT_OF_CONNECTIONS);
        if (listen_res == -1) 
        {
            perror("Listen failed");
        }

        struct sockaddr address;
        socklen_t address_len = sizeof(address);

        while (true)
        {
            int cfd = accept(sfd, &address, &address_len); 
            
            int pid = fork();
            if (pid)
            {
                close(cfd);
            }
            else 
            {
                close(STDIN);
                close(STDOUT);
                close(STDERR);
                close(sfd);
                int master, slave;
                char buf[4096];
                openpty(&master, &slave, buf, NULL, NULL);
                if (fork())
                {
                    close(slave);
                    int buf_size = 4096;
                    char buf[buf_size];
                    int readed;
                    while (true) 
                    {
                        readed = read(master, buf, buf_size);
                        if (readed > 0) 
                        {
                            write(cfd, buf, readed);
                        }
                        readed = read(cfd, buf, buf_size);
                        if (readed > 0) 
                        {
                            write(master, buf, readed);
                        }
                        sleep(2);
                    }
                    close(cfd);
                }
                else
                {
                    setsid();
                    close(master);
                    dup2(slave, STDIN);
                    dup2(slave, STDOUT);
                    dup2(slave, STDERR);
                    close(cfd);
                    int fd = open(buf, O_RDWR);
                    close(fd);
                    execl("/bin/bash", "/bin/bash", NULL);
                }
            }

        }

        close(sfd);
    }
    return 0;
}
