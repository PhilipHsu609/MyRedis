#include "utils.hpp"

#include <dbg.h>        // dbg
#include <fmt/ranges.h> // fmt::print

#include <array>       // std::array
#include <cerrno>      // errno
#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint8_t
#include <cstdio>      // stderr, BUFSIZ
#include <cstring>     // std::strerror, std::memcpy, std::memmove
#include <memory>      // std::unique_ptr
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <netinet/in.h> // sockaddr_in
#include <poll.h>       // poll, pollfd, nfds_t
#include <sys/epoll.h>
#include <sys/socket.h> // accept4, bind, listen, socket, sockaddr
#include <sys/types.h>  // ssize_t
#include <unistd.h>     // close, read, write, socklen_t

enum class State : std::uint8_t { REQUEST = 0, RESPONSE = 1, END = 2 };

struct Connection {
    int fd = -1;
    // Current state of the connection
    State state = State::REQUEST;
    // Reading
    std::size_t rbuf_size = 0;
    std::size_t read_size = 0;
    std::vector<std::byte> rbuf;
    // Writing
    std::size_t wbuf_size = 0;
    std::size_t wbuf_sent = 0;
    std::vector<std::byte> wbuf;

    Connection(int fd) : fd{fd} {
        rbuf.resize(BUFSIZ);
        wbuf.resize(BUFSIZ);
    }
};

void add_connection(std::vector<std::unique_ptr<Connection>> &connections, int fd) {
    if (connections.size() <= static_cast<std::size_t>(fd)) {
        connections.resize(fd + 1);
    }
    connections[fd] = std::make_unique<Connection>(fd);
}

int accept_new_connection(std::vector<std::unique_ptr<Connection>> &connections,
                          int listen_fd) {
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    const int client_fd =
        accept4(listen_fd, reinterpret_cast<sockaddr *>(&addr), &addrlen, SOCK_CLOEXEC);
    if (client_fd == -1) {
        fmt::print(stderr, "accept failed: {}\n", std::strerror(errno));
        return -1;
    }

    set_nonblocking(client_fd);
    add_connection(connections, client_fd);

    return client_fd;
}

bool try_flush_buffer(std::unique_ptr<Connection> &conn) {
    ssize_t n = 0;

    do {
        // Write as much data as possible, up to the buffer size
        const std::size_t remain = conn->wbuf_size - conn->wbuf_sent;
        n = write(conn->fd, conn->wbuf.data() + conn->wbuf_sent, remain);
    } while (n < 0 && errno == EINTR);

    if (n < 0 && errno == EAGAIN) {
        // Resource temporarily unavailable, try again later
        return false;
    }

    if (n < 0) {
        fmt::print(stderr, "write failed: {}\n", std::strerror(errno));
        conn->state = State::END;
        return false;
    }

    conn->wbuf_sent += n;

    if (conn->wbuf_sent == conn->wbuf_size) {
        // Response was fully sent
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        conn->state = State::REQUEST;
        return false;
    }

    // Still got data to send
    return true;
}

void state_res(std::unique_ptr<Connection> &conn) {
    while (try_flush_buffer(conn)) {
    }
}

bool try_one_request(std::unique_ptr<Connection> &conn) {
    if (conn->rbuf_size < conn->read_size + COMMAND_SIZE) {
        // Not enough data in the buffer
        return false;
    }

    std::size_t len = 0;
    std::memcpy(&len, conn->rbuf.data() + conn->read_size, COMMAND_SIZE);

    if (len > BUFSIZ) {
        fmt::print(stderr, "invalid length: {}\n", len);
        conn->state = State::END;
        return false;
    }

    if (conn->rbuf_size < conn->read_size + COMMAND_SIZE + len) {
        // Not enough data in the buffer
        return false;
    }

    dbg("Received length:", len, "message:", conn->rbuf);

    // Flush the write buffer if it's full
    if (conn->wbuf_size + COMMAND_SIZE + len > conn->wbuf.size()) {
        conn->state = State::RESPONSE;
        state_res(conn);
    }

    // Prepare echoing response
    // Pipe the responses to the write buffer
    std::memcpy(conn->wbuf.data() + conn->wbuf_size, &len, COMMAND_SIZE);
    std::memcpy(conn->wbuf.data() + conn->wbuf_size + COMMAND_SIZE,
                conn->rbuf.data() + conn->read_size + COMMAND_SIZE, len);
    conn->wbuf_size += COMMAND_SIZE + len;

    conn->read_size += COMMAND_SIZE + len;

    return conn->state == State::REQUEST;
}

bool try_fill_buffer(std::unique_ptr<Connection> &conn) {
    if (conn->read_size > 0) {
        // Remove handled requests from the buffer
        const std::size_t remain = conn->rbuf_size - conn->read_size;
        std::memmove(conn->rbuf.data(), conn->rbuf.data() + conn->read_size, remain);
        conn->rbuf_size = remain;
        conn->read_size = 0;
    }

    ssize_t n = 0;

    do {
        // Read as much data as possible, up to the buffer size
        const std::size_t remain = conn->rbuf.size() - conn->rbuf_size;
        n = read(conn->fd, conn->rbuf.data() + conn->rbuf_size, remain);
    } while (n == -1 && errno == EINTR); // Interrupt occurred before read

    if (n == -1 && errno == EAGAIN) {
        // Resource temporarily unavailable, try again later
        return false;
    }

    if (n <= 0) {
        // Error or EOF
        fmt::print(stderr, "read failed: {}\n", std::strerror(errno));
        conn->state = State::END;
        return false;
    }

    conn->rbuf_size += n;

    // Process requests one by one
    while (try_one_request(conn)) {
    }

    // Change the state to response
    conn->state = State::RESPONSE;
    state_res(conn);

    return conn->state == State::REQUEST;
}

void state_req(std::unique_ptr<Connection> &conn) {
    while (try_fill_buffer(conn)) {
    }
}

void connection_io(std::unique_ptr<Connection> &conn) {
    if (conn->state == State::REQUEST) {
        state_req(conn);
    } else if (conn->state == State::RESPONSE) {
        state_res(conn);
    }
}

int main() {
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        fmt::print(stderr, "socket failed: {}\n", std::strerror(errno));
        return 1;
    }

    int val = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        fmt::print(stderr, "setsockopt failed: {}\n", std::strerror(errno));
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(0);

    if (bind(listen_fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1) {
        fmt::print(stderr, "bind failed: {}\n", std::strerror(errno));
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        fmt::print(stderr, "listen failed: {}\n", std::strerror(errno));
        return 1;
    }

    fmt::print("Listening on port {}\n", PORT);

    set_nonblocking(listen_fd);
    std::vector<std::unique_ptr<Connection>> connections; // index is fd
    std::array<epoll_event, MAX_EVENTS> events{};

    const int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        fmt::print(stderr, "epoll_create1 failed: {}\n", std::strerror(errno));
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;

    // Add the listening socket to the epoll set
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        dbg(fmt::format("epoll_ctl failed: {}", std::strerror(errno)));
        return 1;
    }

    while (true) {
        const int nready = epoll_wait(epfd, events.data(), MAX_EVENTS, -1);
        if (nready < 0) {
            fmt::print(stderr, "epoll_wait failed: {}\n", std::strerror(errno));
            return 1;
        }

        dbg(nready);
        for (int i = 0; i < nready; ++i) {
            if (events[i].data.fd == listen_fd) {
                const int client_fd = accept_new_connection(connections, listen_fd);

                // Add the new connection to the epoll set
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    dbg(fmt::format("epoll_ctl failed: {}", std::strerror(errno)));
                    return 1;
                }
            } else {
                auto &conn = connections[events[i].data.fd];
                connection_io(conn);
                if (conn->state == State::END) {
                    close(conn->fd);
                    conn.reset();
                }
            }
        }
    }
}