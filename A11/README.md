# Assignment 11: A Simple Group Chat Server with Fuzzing Clients

In this assignment, you will write a simple group chat server program and a client program. The
server will accept multiple clients and relay client messages back to the clients. Each client, on
the other hand, will be a fuzzer generating messages and sending and receiving those messages to and
from the server.

Before you start the assignment, make sure you `record`. Missing recordings (`.record/` files) in
your submission is considered an academic integrity violation and will follow the course policy
accordingly. While recording, do not copy and paste anything. If you do, your submission will be
flagged for the instructor's review, which may result in a 0 for the assignment or an academic
integrity violation. If you happen to do it, undo it immediately and your submission will not be
flagged. Neovim's own copy-paste operations (e.g., `y` and `p`) are fine.

## Group Chat Server

The group chat server accepts client connections, receives client messages, and echoes those
messages back to all clients. The following are the requirements of the server.

* The command-line interface should be able to start the server as follows.

  ```bash
  $ ./server <port number> <# of clients>
  ```

* The executable of your server should be named `server`.
* The server should bind to port `port number`.
* `<# of clients>` is the expected number of clients.
    * A client can join any time and leave any time. The server must keep working for the remaining
      clients when a client leaves.
    * The server terminates after it receives a type `1` message from `<# of clients>` clients (see
      [Two-Phase Commit Protocol](#two-phase-commit-protocol)).
    * You do not need to handle cases where this number is just too big and our container cannot
      handle that many clients. We will not test with more than 100 clients.
* The server should use `AF_INET`.
* The server should bind to all available IP addresses for the local host.
* The server should receive all messages from each and every client as they send them.
* When the server receives a message from a client, it should send the message to *all* clients,
  *including* the sender.
* Message ordering
    * When a client sends multiple messages, all clients should receive the messages in the original
      sending order.
    * In addition, all clients should receive all messages in the same order.
        * For example, suppose we have three clients, C0, C1, and C2.
        * Consider the following scenario.
            * C0 sends a message, M0, which is received by the server.
            * C1 then sends a message, M1, which is also received by the server.
            * Lastly, C2 sends a message, M2, which is again received by the server.
        * Based on the above scenario, C0, C1, and C2 should all receive M0 first, M1 next, and then
          M2.
    * This message ordering is not a trick requirement. It is just what you expect from a group chat
      room.
* Note: the simplest way to test that a server accepts connections is to use `telnet <IP> <port>`.
    * Suppose your local IP address is `192.168.68.6` and you run the server, which binds to port
      `8000`.
    * `telnet 192.168.68.6 8000` will act as a client connected to the server.
    * `127.0.0.1` is a local IP address that refers to the local machine, so `telnet 127.0.0.1 8000`
      will also work.
    * `localhost` is a host name that refers to the local machine, so `telnet localhost 8000` works
      as well.
    * You can enter messages on your terminal as a standard input to `telnet` and they will be sent
      to the server.
    * `telnet` sends plain text, not the binary type byte required by the [messaging
      protocol](#messaging-protocol), so it cannot be used to test the protocol itself.

## Messaging Protocol

The following is how the server and the clients should format and interpret messages they send and
receive.

* For clarity, we use two terms---a *group chat message* and a *message*.
    * A *group chat message* refers to what we would normally call a "message" in the context of a
      group chat app.
    * A *message* is a piece of data sent by a client or a server through a socket. As described
      below, this may or may not contain a group chat message.
* A message sent by a client should be no more than 1024 bytes, including the type byte and `'\n'`.
* Both for the server side and the client side, a message should always start with a one-byte
  (`uint8_t`) "type."
    * `0` means that it is a regular message that contains a group chat message.
        * We refer to this as a type `0` message.
    * `1` means that it is a special "end of execution" message.
        * [Two-Phase Commit Protocol](#two-phase-commit-protocol) below fully explains this.
        * We refer to this as a type `1` message.
* Both for the server and the clients, a new line character `'\n'` always marks the end of a
  message.
    * This means that when you send out a message, it's best to make sure you send up to `'\n'`. If
      you blindly send out `x` bytes since that is the size of your buffer, you will have a hard
      time making things work. The same goes for receiving a message. It's best to make sure that
      you receive up to `'\n'` and handle the message.
    * In addition, you need to be careful with other aspects of this due to the requirement
      described next.
* When the server receives a type `0` message from a client, the server should get the *sender's*
  address (IP and port) and include the address in the message sent to all clients.
    * Consider a typical group chat app. When a client sends a group chat message, everyone should
      know who sent it. We achieve this by having the server add the sender's address to each group
      chat message.
    * The IP address should be of type `uint32_t`. It should come after the first byte that
      indicates the type (`0`).
    * The port number should be of type `uint16_t`. It should come after the IP address.
    * Note that only the server performs this address inclusion. A client does not do this.
    * Also note that the address and the port number are numbers, `uint32_t` and `uint16_t`,
      respectively. They are not strings. This means that those numbers can contain a byte value
      that is `'\n'`, which, in our protocol, indicates the end of a message. You need to be careful
      with this when you send and receive messages.
* In summary, when the server receives a message from a client, it should interpret the first 1 byte
  as the type. If it is `0`, then the server should interpret the rest of the message as a group
  chat message until it reads `'\n'`.
    * If it is `1`, the server should follow the description in [Two-Phase Commit
      Protocol](#two-phase-commit-protocol) below.
* Similarly, when a client receives a message from the server, it should interpret the first 1 byte
  as the type. If it is `0`, then the client should interpret the next 4 bytes as the original
  sender's IP address, the next 2 bytes as the original sender's port number, and the rest as a
  group chat message until it reads `'\n'`.
    * If it is `1`, the client should follow the description in [Two-Phase Commit
      Protocol](#two-phase-commit-protocol) below.

### Message Formats

The diagrams below show the exact bytes of each message. Numbers above each diagram are byte
offsets. Fields are tightly packed: there is no padding between fields and nothing is sent after
the `'\n'` (no `'\0'`).

**Type `0` message, client to server**

```
 0          1                                        N
+----------+----------------------------------------+----------+
|  type    |  group chat message                    |   '\n'   |
|  0x00    |  (variable length, no '\n' inside)     |   0x0A   |
| uint8_t  |                                        |  1 byte  |
+----------+----------------------------------------+----------+
```

**Type `0` message, server to client**

```
 0          1                     5           7                          N
+----------+---------------------+-----------+--------------------------+----------+
|  type    |  sender IP address  |  sender   |  group chat message      |   '\n'   |
|  0x00    |                     |  port     |  (copied from client)    |   0x0A   |
| uint8_t  |  uint32_t           |  uint16_t |                          |  1 byte  |
+----------+---------------------+-----------+--------------------------+----------+
```

**Type `1` message, client to server *and* server to client**

```
 0          1
+----------+----------+
|  type    |   '\n'   |
|  0x01    |   0x0A   |
| uint8_t  |  1 byte  |
+----------+----------+
```

A type `1` message is always exactly 2 bytes. The server does *not* add an address to it.

**Byte order**

* The sender IP address and port are in **network byte order** (big-endian).
* When a client prints a received message, it should convert the IP address to a string and the port
  to host byte order.

**Example**

Suppose a client at `192.168.1.10`, port `8000`, sends the group chat message `AB`.

```
Client to server (4 bytes):

Offset:  0      1  2     3
Bytes:   00     41 42    0A
Field:   type   "AB"     '\n'

Server to client (10 bytes):

Offset:  0      1  2  3  4        5  6     7  8     9
Bytes:   00     C0 A8 01 0A       1F 40    41 42    0A
Field:   type   192.168.1.10      8000     "AB"     '\n'
```

Notice that this example IP address contains the byte `0x0A`, which is the same value as `'\n'`.

## Fuzzing Clients

The client program is a simple custom fuzzer that generates messages and sends those messages to the
server. It also receives messages sent by the server. It is a custom fuzzer in the sense that it
does not use a fuzzing framework such as [libFuzzer](https://llvm.org/docs/LibFuzzer.html) discussed
in A5. The following are the requirements.

* The command-line interface should be able to start a client as follows.

  ```bash
  $ ./client <IP address> <port number> <# of messages> <log file path>
  ```

* The executable should be named `client`.
* `IP` and `port` are the IP address and the port number of the server, respectively.
* A client should follow the above [message protocol](#messaging-protocol) when sending and
  receiving messages.
* After it starts, a client should keep generating and sending random messages. `<# of messages>`
  should be the number of messages sent.
    * The random messages should be of type `0`.
    * After sending `<# of messages>` messages, the client should stop sending type `0` messages,
      and prepare for termination as described below in [Two-Phase Commit
      Protocol](#two-phase-commit-protocol).
    * You can use `getentropy()` to generate random bytes. Read `man getentropy` to understand how
      to use it.
    * Since `getentropy()` gives you random bytes that you may not be able to print out, use the
      following function to convert it to a hex string.

      ```c
      #include <stdint.h>
      #include <stdio.h>
      #include <unistd.h>

      /*
       * `buf` should point to an array that contains random bytes to convert to a
       * hex string.
       *
       * `str` should point to a buffer used to return the hex string of the random
       * bytes. The size of the buffer should be twice the size of the random bytes
       * (since a byte is two characters in hex) plus one for NULL.
       *
       * `buf_size` is the size of the `buf` array, and `str_size` is the size of the `str` buffer.
       *
       * For example,
       *
       *   uint8_t buf[10];
       *   char str[10 * 2 + 1];
       *   getentropy(buf, 10);
       *   if (convert(buf, sizeof(buf), str, sizeof(str)) != 0) {
       *     exit(EXIT_FAILURE);
       *   }
       */
      int convert(uint8_t *buf, ssize_t buf_size, char *str, ssize_t str_size) {
        if (buf == NULL || str == NULL || buf_size <= 0 ||
            str_size < (buf_size * 2 + 1)) {
          return -1;
        }

        for (int i = 0; i < buf_size; i++)
          sprintf(str + i * 2, "%02X", buf[i]);
        str[buf_size * 2] = '\0';

        return 0;
      }
      ```

* A client should print out each and every type `0` message it receives from the server and store
  them in a file named `<log file path>`.
    * Use this format: `"%-15s%-10u%s"`, e.g., `printf("%-15s%-10u%s", ip, port, str);`
    * For example, `192.167.128.205     9000      9391DE3E275ADB19637D`
    * Each message must be exactly one line in the log file, ending with a single `'\n'`. The log
      file must contain nothing else. The testers compare your log file line by line.
* Note that a client should be able to send messages and receive messages concurrently.

## Two-Phase Commit Protocol

To gracefully end the execution for both the server and the clients, the server and the clients
should collectively implement a simplified [two-phase commit
protocol](https://en.wikipedia.org/wiki/Two-phase_commit_protocol) signaling the end of execution as
follows.

* After sending `# of messages` messages, each client should send a type `1` message to the server
  (as opposed to a type `0` message described in [Messaging Protocol](#messaging-protocol)). This
  is to indicate that the client is done sending messages.
* After sending its type `1` message, a client must continue to receive and log type `0` messages,
  since other clients may still be sending.
* After the server receives a type `1` message from each and every client, it should send a type `1`
  message to all clients. It should then terminate itself.
* Each client should terminate after receiving a type `1` message from the server.

## Code Structure and CMake

* You need to use the same code structure as previous assignments with `src/` and `include/`.
* You also need to write `CMakeLists.txt` that produces two executables, `server` and `client`. **Be
  very careful about file names and capitalization**. If they are not exactly what we expect, it
  will result in a 0.

## Grading

* **Our base grading does not use the debug option or sanitizers.** Therefore, your code should
  function correctly without them.
* You can use `server-tester.<arch>` and `client-tester.<arch>` under `bin/` to test your program.
  If you're using an x86-64 Docker container, use `server-tester.amd64` and `client-tester.amd64`.
  If you are using an ARM64 Docker container, use `server-tester.arm64` and `client-tester.arm64`.
  These executables are essentially the same as the ones we use for grading, though they may not be
  exactly the same. Below, we will generally refer to them as `server-tester` and `client-tester`.
* You *should not* use these executables as debuggers. They do not tell you what the problems are.
  They do provide some error messages but those are not intended to serve as debug messages. They
  mostly just provide only a simple pass/fail result.
* When a test case does not pass, you need to construct the same scenario and debug your program to
  find the problems.
* Testing your server program: `server-tester` is the program that tests your server program. The
  following is how you run the program.

  ```bash
  $ ./server-tester <IP address> <port number> <# of clients> <# of messages> <test #>
  ```

    * You need to run your server program first before running `server-tester`. Start a new server
      for each test, using the same `<# of clients>` as `server-tester` uses for that test. Tests
      `0` to `3` always use one client.
    * `IP address` and `port number` are the IP address and the port number of the server,
      respectively.
    * `<# of messages>` is the number of messages sent by each client. Some tests may ignore this
      argument as they do not need more than one message.
    * `<# of clients>` is the number of clients that start. Some tests may ignore this argument as
      they do not need more than one client.
    * `<test #>` is the test number. The following are the test numbers and their descriptions.
        * `0` [5 pts]: One client sends one type `0` message and receives some message back.
        * `1` [10 pts]: One client sends one type `0` message and receives the server's response
          message back in the correct format (i.e., according to the protocol with 0, IP, port, and
          chat message).
        * `2` [10 pts]: One client keeps sending type `0` messages and receives those messages back
          in the correct format and in the original sending order. We will test this with a large `#
          of messages` (e.g., 100).
        * `3` [5 pts]: One client sends one type `1` message and receives one type `1` message back
          from the server. The server terminates.
        * `4` [10 pts]: Many clients each send one type `0` message and receive all the messages
          back in the correct format and in the same order. We will test this with a large `# of
          clients` (e.g., 50).
        * `5` [15 pts]: Many clients each keep sending messages and receiving all the messages back
          in the correct format and in the same order. We will test this with a large `# of clients`
          and a large `# of messages` (e.g., 50 clients and 100 messages each).
        * `6` [5 pts]: Many clients each send one type `1` message and all receive one type `1`
          message back from the server. The server terminates.
    * The executable prints out logs as it runs.
        * You can change the log level by adding `LOG_LEVEL=<number>` to the command, e.g.,
          `LOG_LEVEL=0 ./server-tester ...`.
        * The log levels range from 0 to 3. 0 is the most verbose, 3 the least.
        * The default log level is 1.
        * When testing and debugging, you will want to use the log level 0 to get more information
          about the execution.
        * Our grader uses the default log level. Thus, you need to make sure that your submission
          works with the default log level.
        * Note that the log level 0 will print out *a lot* of messages. It is better to redirect it
          to a file (e.g., `LOG_LEVEL=0 ./server-tester ... > test.log 2>&1` and examine the file
          after the execution.
* Testing your client program: `client-tester` is the program that tests your client program. The
  following is how you run the program.

  ```bash
  $ ./client-tester <client executable path> <client log prefix> <port number> <# of clients> <# of messages> <test #>
  ```

    * This is essentially a server but it also executes your client. Thus, the output of your client
      program will be printed out on the terminal, along with the output of the program.
    * `<client executable path>` is the path to your client executable.
    * `<client log prefix>` is the prefix of the log file that your client program generates. The
      program will append `.` and the client number to the prefix to create the log file name (e.g.,
      `log.0`, `log.1`, ...).
    * `port number` is the port number that `client-tester` binds to.
    * `<# of clients>` is the number of clients that you want to start. Some tests may ignore this
      argument as they do not need more than one client.
    * `<# of messages>` is the number of messages sent by each client. Some tests may ignore this as
      they do not need more than one message.
    * Every test waits for all clients to terminate. A client that does not complete the [Two-Phase
      Commit Protocol](#two-phase-commit-protocol) and terminate fails the test.
    * `<test #>` is the test number. The following are the test numbers and their descriptions.
        * `0` [5 pts]: After a single client starts, the server receives at least one type `0`
          message and the client prints the message out correctly.
        * `1` [10 pts]: After a single client starts, the server receives `<# of messages>` random
          type `0` messages and the client prints out all messages correctly.
        * `2` [5 pts]: After a single client sends `<# of messages>` messages, the server receives
          one type `1` message and sends it back. The client terminates.
        * `3` [15 pts]: After many clients start, the server keeps receiving type `0` messages and
          the clients print out all messages correctly.
        * `4` [5 pts]: After receiving `<# of messages>` from each and every client, the server
          receives one type `1` message from them and sends it back. All clients terminate.
    * The executable prints out logs as it runs.
        * You can change the log level by adding `LOG_LEVEL=<number>` to the command, e.g.,
          `LOG_LEVEL=0 ./client-tester ...`.
        * The log levels range from 0 to 3. 0 is the most verbose, 3 the least.
        * The default log level is 1.
        * When testing and debugging, you will want to use the log level 0 to get more information
          about the execution.
        * Our grader uses the default log level. Thus, you need to make sure that your submission
          works with the default log level.
        * Note that the log level 0 will print out *a lot* of messages. It is better to redirect it
          to a file (e.g., `LOG_LEVEL=0 ./client-tester ... > test.log 2>&1` and examine the file
          after the execution.
* Occasionally, and especially before you submit, make sure that you run `policy-review` from your
  assignment directory. It checks for things like missing recordings, paste actions, and use of
  unauthorized tools. Missing recordings (`.record/` files) or Neovim editing activity (`.nvim/`
  files) in your submission is considered an academic integrity violation and will follow the course
  policy accordingly. Other issues detected, such as paste actions or use of unauthorized tools,
  will be flagged for the instructor's review, which may result in a 0 for the assignment or an
  academic integrity violation. If it detects issues you cannot fix, create a file called
  `REFLECTION.md` in your repository and briefly explain what happened. Note that this does not
  guarantee a pardon but will be considered by the instructor.
* Also run `./a11-checker` from your assignment directory before submitting. This is a Python script
  included in the repository. It performs basic checks for CMake build behavior and required
  executables. Read any warnings or errors from the checker and fix them before submitting.
* Code that does not compile with CMake gets a 0.
* Code that does not generate all the required executables (`client` and `server`) gets a 0.
* Memory issues have a penalty of -10 pts. Use appropriate sanitizers and enable the debug option.
  Make sure your code works without the debug option as well. Before you submit, make sure that you
  remove the debug option.
* A wrong code directory structure has a penalty of -10 pts.
* Thread/synchronization issues have a penalty of -10 pts.
* You should not hard-code or customize your implementation tailored to our test cases. Generally,
  you should consider the provided test cases as examples or specific instances of general cases.
