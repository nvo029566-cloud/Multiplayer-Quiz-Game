# 🎮 Kahoot Mini - Multiplayer Quiz Game (TCP / C)

A real-time multiplayer quiz system written in C using POSIX TCP sockets and `select()` for concurrency.
The server broadcasts timed questions, clients answer competitively, and scores are updated with speed-based bonuses.

---

## 🚀 Features

* 👥 Supports multiple clients (min 3 players)
* ⏱️ Countdown timer with live updates
* ⚡ Speed-based scoring system
* 📊 Live leaderboard after each round
* 🎨 Colored UI (ANSI terminal styling)
* 📂 Questions loaded from file (questions.txt)
* ❌ Late answers are rejected
* 🔁 Non-blocking I/O using `select()`

---

## 🛠️ Build

```bash
gcc -Wall -Wextra -O2 server.c -o server
gcc -Wall -Wextra -O2 client.c -o client
```

---

## 🌐 Network Setup

| Machine   | Role   | Command                     |
| --------- | ------ | --------------------------- |
| Machine 1 | Server | `./server 8080`             |
| Machine 2 | Client | `./client <SERVER_IP> 8080` |
| Machine 3 | Client | `./client <SERVER_IP> 8080` |

Example:

```bash
./client 192.168.1.104 8080
```

---

## ▶️ Run

### 1. Start Server

```bash
./server 8080
```

Server waits for at least **3 players** before starting.

---

### 2. Start Clients

```bash
./client <SERVER_IP> 8080
```

Enter your name → wait for game start.

---

## 📡 Protocol Design (Custom)

### Client → Server

```text
NAME|<player_name>
ANSWER|<A/B/C/D>
```

---

### Server → Client

```text
INFO|<message>
START|Game is starting!

QUESTION|<round>|<question>|A)<opt>|B)<opt>|C)<opt>|D)<opt>|<time>

TIMER|<seconds_left>

ROUND|Correct:<X>|Answer:<text>|<player1:result>|<player2:result>...

LEADERBOARD|<rank>|<name>|<score>|...

FINAL|Winner:<name>|Score:<score>
```

---

## 💻 Example Interaction

### Server

```text
Server listening on port 8080
Waiting for at least 3 players...

Alice joined
Bob joined
Quan joined

START|Game is starting!

QUESTION|1|What protocol uses port 53?|A)DNS|B)HTTP|C)FTP|D)SMTP|15

Alice: CORRECT +10
Bob: WRONG
Quan: TIMEOUT

=== LEADERBOARD ===
1. Alice - 10 pts
2. Bob - 0 pts
3. Quan - 0 pts
```

---

### Client UI

```text
========================================
        QUIZZ KAHOOT MINI
========================================

Question 1
What protocol uses port 53?

A. DNS
B. HTTP
C. FTP
D. SMTP

Time left: [██████████████      ] 10 sec
Your answer: A

Correct! +10 points
```

---

## 🏗️ Architecture

```text
            ┌───────────────┐
            │    SERVER     │
            │---------------│
            │ select() loop │
            │ client list   │
            │ question set  │
            │ scoring logic │
            └──────┬────────┘
                   │ TCP
        ┌──────────┼──────────┐
        │          │          │
     Client      Client     Client
```

---

## ⚙️ Core Design

### 🔁 Concurrency

* Single-threaded server
* Uses `select()` to handle:

  * multiple client sockets
  * timed events

---

### ⏱️ Timer System

* Each question has fixed time (`TIME_LIMIT = 15`)
* Server sends `TIMER|x` every second
* Client updates real-time progress bar

---

### ⚡ Scoring System

From your server code:

* Base score: **10 points**
* Speed bonus:

| Time left | Bonus |
| --------- | ----- |
| ≥ 12s     | +5    |
| ≥ 9s      | +4    |
| ≥ 6s      | +3    |
| ≥ 3s      | +2    |
| ≥ 1s      | +1    |

---

### 📊 Leaderboard

* Sorted descending by score
* Updated after every round
* Sent as `LEADERBOARD|rank|name|score`

---

### 🧠 Answer Handling

* Each client can answer **once per question**
* Duplicate answers rejected
* Late answers ignored

---

## 📂 File Structure

```text
quiz-game/
│── server.c
│── client.c
│── questions.txt
```

---

## 📄 Question Format

```text
Question|A|B|C|D|Correct
```

Example:

```text
What protocol uses port 53?|DNS|HTTP|FTP|SMTP|A
```

---

## 🎨 Client UI Features

From your `client.c`:

* ANSI color styling
* Progress bar timer
* Clear screen between questions
* Highlighted leaderboard 🥇🥈🥉

---

## ⚠️ Notes

* Use IP dạng `192.168.x.x`
* Không dùng `127.0.0.1` cho nhiều máy
* Phải cùng mạng (hoặc hotspot)
* Tắt firewall nếu không connect được

---

## 🧹 Cleanup

```bash
rm server client
```

---

## 👨‍💻 Author

* Name: Nam
* Project: Network Programming
* Year: 2025–2026

---

## ⭐ Highlights

* Real-time multiplayer over TCP
* Non-blocking I/O with `select()`
* Speed-based scoring system
* Terminal UI giống Kahoot mini
