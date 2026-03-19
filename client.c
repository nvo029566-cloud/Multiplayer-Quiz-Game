#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctype.h>

#define BUF_SIZE 2048
#define MAX_TEXT 512

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"

void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

int recv_line(int sockfd, char *buffer, int maxlen) {
    int total = 0;
    char c;
    while (total < maxlen - 1) {
        int n = recv(sockfd, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\n') break;
        buffer[total++] = c;
    }
    buffer[total] = '\0';
    return total;
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void print_header() {
    printf(BLUE BOLD);
    printf("========================================\n");
    printf("           QUIZZ KAHOOT MINI            \n");
    printf("========================================\n");
    printf(RESET);
}

void print_timer_bar(int remain, int total) {
    int width = 20;
    int filled = (remain * width) / total;

    printf(YELLOW "\rTime left: [");
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("█");
        else printf(" ");
    }
    printf("] %2d sec" RESET, remain);
    fflush(stdout);
}

void print_info(const char *msg) {
    printf(CYAN "%s\n" RESET, msg);
}

void print_success(const char *msg) {
    printf(GREEN "%s\n" RESET, msg);
}

void print_error(const char *msg) {
    printf(RED "%s\n" RESET, msg);
}

void print_question(const char *round, const char *question,
                    const char *a, const char *b,
                    const char *c, const char *d,
                    const char *limit) {
    clear_screen();
    print_header();

    printf(MAGENTA BOLD "\nQuestion %s\n" RESET, round);
    printf(WHITE "%s\n\n" RESET, question);

    printf(GREEN  "A. %s\n" RESET, a);
    printf(YELLOW "B. %s\n" RESET, b);
    printf(BLUE   "C. %s\n" RESET, c);
    printf(RED    "D. %s\n" RESET, d);

    printf("\n");
    print_timer_bar(atoi(limit), atoi(limit));
    printf("\n");
    printf(BOLD "Your answer (A/B/C/D): " RESET);
    fflush(stdout);
}

void print_round_result(const char *msg) {
    printf("\n\n" MAGENTA BOLD "========== ROUND RESULT ==========\n" RESET);

    char copy[BUF_SIZE];
    strncpy(copy, msg, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *tok = strtok(copy, "|"); // ROUND
    while ((tok = strtok(NULL, "|")) != NULL) {
        printf("%s\n", tok);
    }
}

void print_leaderboard(const char *msg) {
    printf(MAGENTA BOLD "\n=========== LEADERBOARD ===========\n" RESET);

    char copy[BUF_SIZE];
    strncpy(copy, msg, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    strtok(copy, "|"); // LEADERBOARD

    char *rank, *name, *score;
    while ((rank = strtok(NULL, "|")) != NULL &&
           (name = strtok(NULL, "|")) != NULL &&
           (score = strtok(NULL, "|")) != NULL) {

        if (strcmp(rank, "1") == 0)
            printf("🥇 %s - %s pts\n", name, score);
        else if (strcmp(rank, "2") == 0)
            printf("🥈 %s - %s pts\n", name, score);
        else if (strcmp(rank, "3") == 0)
            printf("🥉 %s - %s pts\n", name, score);
        else
            printf("%s. %s - %s pts\n", rank, name, score);
    }
}

void print_final(const char *msg) {
    printf(MAGENTA BOLD "\n============= GAME OVER =============\n" RESET);

    char copy[BUF_SIZE];
    strncpy(copy, msg, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    strtok(copy, "|"); // FINAL
    char *winner = strtok(NULL, "|");
    char *score = strtok(NULL, "|");

    if (winner) printf(GREEN "%s\n" RESET, winner);
    if (score) printf(YELLOW "%s\n" RESET, score);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        printf("Invalid server IP.\n");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    clear_screen();
    print_header();

    char name[100];
    printf(BOLD "Enter name: " RESET);
    fgets(name, sizeof(name), stdin);
    trim_newline(name);

    char out[BUF_SIZE];
    snprintf(out, sizeof(out), "NAME|%s\n", name);
    send(sockfd, out, strlen(out), 0);

    int in_question = 0;
    int total_time = 15;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            char line[BUF_SIZE];
            int n = recv_line(sockfd, line, sizeof(line));
            if (n <= 0) {
                printf("\nDisconnected from server.\n");
                break;
            }

            if (strncmp(line, "INFO|", 5) == 0) {
                printf("\n");
                print_info(line + 5);
                if (in_question) {
                    printf(BOLD "Your answer (A/B/C/D): " RESET);
                    fflush(stdout);
                }
            } else if (strncmp(line, "START|", 6) == 0) {
                clear_screen();
                print_header();
                print_success(line + 6);
            } else if (strncmp(line, "QUESTION|", 9) == 0) {
                char copy[BUF_SIZE];
                strncpy(copy, line, sizeof(copy) - 1);
                copy[sizeof(copy) - 1] = '\0';

                char *tok = strtok(copy, "|"); // QUESTION
                char *round = strtok(NULL, "|");
                char *question = strtok(NULL, "|");
                char *a = strtok(NULL, "|");
                char *b = strtok(NULL, "|");
                char *c = strtok(NULL, "|");
                char *d = strtok(NULL, "|");
                char *limit = strtok(NULL, "|");

                total_time = limit ? atoi(limit) : 15;
                in_question = 1;

                print_question(round ? round : "?",
                               question ? question : "",
                               a ? a : "",
                               b ? b : "",
                               c ? c : "",
                               d ? d : "",
                               limit ? limit : "15");
            } else if (strncmp(line, "TIMER|", 6) == 0) {
                int remain = atoi(line + 6);
                printf("\r");
                print_timer_bar(remain, total_time);
                if (remain == 0) {
                    printf("\n");
                } else {
                    printf("\n" BOLD "Your answer (A/B/C/D): " RESET);
                    fflush(stdout);
                }
            } else if (strncmp(line, "ROUND|", 6) == 0) {
                in_question = 0;
                printf("\n");
                print_round_result(line);
            } else if (strncmp(line, "LEADERBOARD", 11) == 0) {
                print_leaderboard(line);
                printf("\n");
            } else if (strncmp(line, "FINAL|", 6) == 0) {
                print_final(line);
                break;
            } else {
                printf("%s\n", line);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds) && in_question) {
            char input[64];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                trim_newline(input);
                if (strlen(input) > 0) {
                    char ans = (char)toupper((unsigned char)input[0]);
                    if (ans >= 'A' && ans <= 'D') {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "ANSWER|%c\n", ans);
                        send(sockfd, msg, strlen(msg), 0);
                    } else {
                        print_error("Please enter only A, B, C, or D.");
                        printf(BOLD "Your answer (A/B/C/D): " RESET);
                        fflush(stdout);
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
