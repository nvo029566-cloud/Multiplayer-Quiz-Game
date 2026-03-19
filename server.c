#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include <ctype.h>

#define MAX_CLIENTS 10
#define MIN_CLIENTS 3
#define MAX_QUESTIONS 100
#define MAX_TEXT 512
#define MAX_NAME 50
#define BUF_SIZE 2048
#define TIME_LIMIT 15
#define BASE_POINTS 10

typedef struct {
    char question[MAX_TEXT];
    char options[4][MAX_TEXT];
    char correct; // A/B/C/D
} Question;

typedef struct {
    int sockfd;
    char name[MAX_NAME];
    int score;
    int connected;
    int answered;
    char answer[16];
    int answer_time;
} Client;

static Client clients[MAX_CLIENTS];
static Question questions[MAX_QUESTIONS];
static int question_count = 0;

void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

void trim_spaces(char *s) {
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);

    int end = (int)strlen(s) - 1;
    while (end >= 0 && (s[end] == ' ' || s[end] == '\t')) {
        s[end] = '\0';
        end--;
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

int send_line(int sockfd, const char *msg) {
    char out[BUF_SIZE];
    snprintf(out, sizeof(out), "%s\n", msg);
    return send(sockfd, out, strlen(out), 0);
}

void broadcast_line(const char *msg) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) {
            send_line(clients[i].sockfd, msg);
        }
    }
}

int load_questions(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen questions");
        return 0;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp) && question_count < MAX_QUESTIONS) {
        trim_newline(line);
        if (strlen(line) == 0) continue;

        char *parts[6];
        int count = 0;

        char *token = strtok(line, "|");
        while (token && count < 6) {
            parts[count++] = token;
            token = strtok(NULL, "|");
        }

        if (count != 6) continue;

        strncpy(questions[question_count].question, parts[0], MAX_TEXT - 1);
        questions[question_count].question[MAX_TEXT - 1] = '\0';

        for (int i = 0; i < 4; i++) {
            strncpy(questions[question_count].options[i], parts[i + 1], MAX_TEXT - 1);
            questions[question_count].options[i][MAX_TEXT - 1] = '\0';
            trim_spaces(questions[question_count].options[i]);
        }

        trim_spaces(questions[question_count].question);
        trim_spaces(parts[5]);
        questions[question_count].correct = (char)toupper((unsigned char)parts[5][0]);

        question_count++;
    }

    fclose(fp);
    return question_count;
}

int count_connected_clients() {
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) count++;
    }
    return count;
}

void reset_answers() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) {
            clients[i].answered = 0;
            clients[i].answer[0] = '\0';
            clients[i].answer_time = -1;
        }
    }
}

int speed_bonus(int elapsed) {
    int remain = TIME_LIMIT - elapsed;
    if (remain >= 12) return 5;
    if (remain >= 9) return 4;
    if (remain >= 6) return 3;
    if (remain >= 3) return 2;
    if (remain >= 1) return 1;
    return 0;
}

void disconnect_client(int idx) {
    if (clients[idx].connected) {
        close(clients[idx].sockfd);
        clients[idx].connected = 0;
        printf("Client %s disconnected.\n", clients[idx].name);
    }
}

void build_leaderboard(char *out, size_t out_size) {
    typedef struct {
        char name[MAX_NAME];
        int score;
    } Rank;

    Rank ranks[MAX_CLIENTS];
    int n = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) {
            strncpy(ranks[n].name, clients[i].name, MAX_NAME - 1);
            ranks[n].name[MAX_NAME - 1] = '\0';
            ranks[n].score = clients[i].score;
            n++;
        }
    }

    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ranks[j].score > ranks[i].score) {
                Rank tmp = ranks[i];
                ranks[i] = ranks[j];
                ranks[j] = tmp;
            }
        }
    }

    snprintf(out, out_size, "LEADERBOARD");
    for (int i = 0; i < n; i++) {
        char line[128];
        snprintf(line, sizeof(line), "|%d|%s|%d", i + 1, ranks[i].name, ranks[i].score);
        strncat(out, line, out_size - strlen(out) - 1);
    }
}

void announce_round_result(Question *q) {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "ROUND|Correct:%c|Answer:%s",
             q->correct, q->options[q->correct - 'A']);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].connected) continue;

        char part[256];
        if (!clients[i].answered) {
            snprintf(part, sizeof(part), "|%s:TIMEOUT", clients[i].name);
        } else {
            char user_ans = (char)toupper((unsigned char)clients[i].answer[0]);
            if (user_ans == q->correct) {
                int pts = BASE_POINTS + speed_bonus(clients[i].answer_time);
                snprintf(part, sizeof(part), "|%s:CORRECT:+%d", clients[i].name, pts);
            } else {
                snprintf(part, sizeof(part), "|%s:WRONG", clients[i].name);
            }
        }
        strncat(msg, part, sizeof(msg) - strlen(msg) - 1);
    }

    broadcast_line(msg);

    char board[BUF_SIZE];
    build_leaderboard(board, sizeof(board));
    broadcast_line(board);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc >= 2) port = atoi(argv[1]);

    if (!load_questions("questions.txt")) {
        printf("No valid questions loaded from questions.txt\n");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d\n", port);
    printf("Waiting for at least %d players...\n", MIN_CLIENTS);

    memset(clients, 0, sizeof(clients));

    while (count_connected_clients() < MIN_CLIENTS) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (new_sock < 0) {
            perror("accept");
            continue;
        }

        char line[BUF_SIZE];
        int n = recv_line(new_sock, line, sizeof(line));
        if (n <= 0) {
            close(new_sock);
            continue;
        }

        if (strncmp(line, "NAME|", 5) != 0) {
            send_line(new_sock, "INFO|Invalid first message. Use NAME|your_name");
            close(new_sock);
            continue;
        }

        char name[MAX_NAME];
        strncpy(name, line + 5, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';
        trim_spaces(name);

        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].connected) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {
            send_line(new_sock, "INFO|Server full");
            close(new_sock);
            continue;
        }

        clients[slot].sockfd = new_sock;
        clients[slot].connected = 1;
        clients[slot].score = 0;
        clients[slot].answered = 0;
        clients[slot].answer_time = -1;
        clients[slot].answer[0] = '\0';
        strncpy(clients[slot].name, name, MAX_NAME - 1);
        clients[slot].name[MAX_NAME - 1] = '\0';

        printf("%s joined from %s\n", clients[slot].name, inet_ntoa(client_addr.sin_addr));

        char welcome[128];
        snprintf(welcome, sizeof(welcome), "INFO|Welcome %s. Waiting for game start...", clients[slot].name);
        send_line(new_sock, welcome);
    }

    broadcast_line("START|Game is starting!");

    for (int q = 0; q < question_count; q++) {
        if (count_connected_clients() == 0) break;

        reset_answers();

        char qmsg[4096];
        snprintf(qmsg, sizeof(qmsg),
                 "QUESTION|%d|%s|A)%s|B)%s|C)%s|D)%s|%d",
                 q + 1,
                 questions[q].question,
                 questions[q].options[0],
                 questions[q].options[1],
                 questions[q].options[2],
                 questions[q].options[3],
                 TIME_LIMIT);

        broadcast_line(qmsg);

        time_t start = time(NULL);
        time_t end_time = start + TIME_LIMIT;
        int last_broadcast = TIME_LIMIT;

        while (1) {
            time_t now = time(NULL);
            int remain = (int)(end_time - now);
            if (remain < 0) remain = 0;

            if (remain != last_broadcast) {
                last_broadcast = remain;
                char tmsg[64];
                snprintf(tmsg, sizeof(tmsg), "TIMER|%d", remain);
                broadcast_line(tmsg);
            }

            if (remain <= 0) break;

            fd_set readfds;
            FD_ZERO(&readfds);
            int maxfd = -1;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].connected) {
                    FD_SET(clients[i].sockfd, &readfds);
                    if (clients[i].sockfd > maxfd) maxfd = clients[i].sockfd;
                }
            }

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200000; // 0.2s

            int activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);
            if (activity < 0) {
                if (errno == EINTR) continue;
                perror("select");
                break;
            }

            if (activity == 0) continue;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].connected) continue;
                if (!FD_ISSET(clients[i].sockfd, &readfds)) continue;

                char line[BUF_SIZE];
                int n = recv_line(clients[i].sockfd, line, sizeof(line));
                if (n <= 0) {
                    disconnect_client(i);
                    continue;
                }

                if (strncmp(line, "ANSWER|", 7) == 0) {
                    if (clients[i].answered) {
                        send_line(clients[i].sockfd, "INFO|You already answered this question.");
                        continue;
                    }

                    char ans[16];
                    strncpy(ans, line + 7, sizeof(ans) - 1);
                    ans[sizeof(ans) - 1] = '\0';
                    trim_spaces(ans);

                    char letter = (char)toupper((unsigned char)ans[0]);
                    if (letter < 'A' || letter > 'D') {
                        send_line(clients[i].sockfd, "INFO|Please answer with A, B, C, or D.");
                        continue;
                    }

                    clients[i].answered = 1;
                    clients[i].answer_time = (int)(time(NULL) - start);
                    snprintf(clients[i].answer, sizeof(clients[i].answer), "%c", letter);

                    if (letter == questions[q].correct) {
                        int pts = BASE_POINTS + speed_bonus(clients[i].answer_time);
                        clients[i].score += pts;

                        char ok[128];
                        snprintf(ok, sizeof(ok), "INFO|Correct! +%d points", pts);
                        send_line(clients[i].sockfd, ok);
                    } else {
                        send_line(clients[i].sockfd, "INFO|Wrong answer.");
                    }
                } else {
                    send_line(clients[i].sockfd, "INFO|Unknown message.");
                }
            }
        }

        announce_round_result(&questions[q]);
        sleep(2);
    }

    int best_score = -1;
    char winner[MAX_NAME] = "No one";

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected && clients[i].score > best_score) {
            best_score = clients[i].score;
            strncpy(winner, clients[i].name, sizeof(winner) - 1);
            winner[sizeof(winner) - 1] = '\0';
        }
    }

    char final_msg[256];
    snprintf(final_msg, sizeof(final_msg), "FINAL|Winner:%s|Score:%d", winner, best_score);
    broadcast_line(final_msg);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) close(clients[i].sockfd);
    }
    close(server_fd);
    return 0;
}
