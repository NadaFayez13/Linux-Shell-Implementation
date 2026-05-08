# 🐧 Custom Linux Shell (myShell) 

A robust, functional command-line interface implemented in C, simulating real Linux shell behaviors like process management and command chaining.

---

## 🚀 Key Features
* **Process Control:** Full management of external commands using `fork()`, `execvp()`, and `waitpid()`.
* **I/O Redirection:** Advanced file stream handling using `>` (output) and `<` (input).
* **Piping:** Seamless command chaining using `pipe()` for complex operations (e.g., `ls | grep .c`).
* **Background Execution:** Support for asynchronous tasks using the `&` operator.
* **Signal Handling:** Custom handlers for `SIGINT` (Ctrl+C) to ensure shell stability.
* **Built-in Commands:** Native support for `cd`, `pwd`, `history`, and `exit`.

---

## 🛠️ Technical Stack
* **Language:** C
* **Environment:** Linux / POSIX API
* **Build Tool:** Makefile

---

## 💻 Installation & Usage
1. Clone the repository:
   ```bash
   git clone [https://github.com/NadaFayez13/Linux-Shell-Implementation]
