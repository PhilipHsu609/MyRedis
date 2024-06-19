#include "utils.hpp"

#include <dbg.h>        // dbg
#include <fmt/ranges.h> // fmt::print

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

std::int32_t accept_new_connection(std::vector<std::unique_ptr<Connection>> &connections,
                                   int fd) {
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    const int client_fd =
        accept4(fd, reinterpret_cast<sockaddr *>(&addr), &addrlen, SOCK_CLOEXEC);
    if (client_fd == -1) {
        fmt::print(stderr, "accept failed: {}\n", std::strerror(errno));
        return -1;
    }

    set_nonblocking(client_fd);
    add_connection(connections, client_fd);

    return 0;
}

bool try_flush_buffer(std::unique_ptr<Connection> &conn) {
    ssize_t n = 0;

    do {
        // Write as much data as possible, up to the buffer size
        const std::size_t remain = conn->wbuf_size - conn->wbuf_sent;
        n = write(conn->fd, conn->wbuf.data() + conn->wbuf_sent, remain);
        dbg("write", n, "bytes");
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
    if (conn->rbuf_size < COMMAND_SIZE) {
        // Not enough data in the buffer
        return false;
    }

    std::size_t len = 0;
    std::memcpy(&len, conn->rbuf.data(), COMMAND_SIZE);

    if (len > BUFSIZ) {
        fmt::print(stderr, "invalid length: {}\n", len);
        conn->state = State::END;
        return false;
    }

    if (conn->rbuf_size < COMMAND_SIZE + len) {
        // Not enough data in the buffer
        return false;
    }

    dbg("Received length:", len, "message:", conn->rbuf);

    // Prepare echoing response
    std::memcpy(conn->wbuf.data(), &len, COMMAND_SIZE);
    std::memcpy(conn->wbuf.data() + COMMAND_SIZE, conn->rbuf.data() + COMMAND_SIZE, len);
    conn->wbuf_size = COMMAND_SIZE + len;

    // Remove the request from the buffer
    const std::size_t remain = conn->rbuf_size - COMMAND_SIZE - len;
    if (remain > 0) {
        std::memmove(conn->rbuf.data(), conn->rbuf.data() + COMMAND_SIZE + len, remain);
    }
    conn->rbuf_size = remain;

    // Change the state to response
    conn->state = State::RESPONSE;
    state_res(conn);

    return conn->state == State::REQUEST;
}

bool try_fill_buffer(std::unique_ptr<Connection> &conn) {
    ssize_t n = 0;

    do {
        // Read as much data as possible, up to the buffer size
        const std::size_t remain = conn->rbuf.size() - conn->rbuf_size;
        n = read(conn->fd, conn->rbuf.data() + conn->rbuf_size, remain);
    } while (n < 0 && errno == EINTR); // Interrupt occurred before read

    if (n < 0 && errno == EAGAIN) {
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

    while (try_one_request(conn)) {
        // Process requests one by one
    }

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
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fmt::print(stderr, "socket failed: {}\n", std::strerror(errno));
        return 1;
    }

    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0) {
        fmt::print(stderr, "setsockopt failed: {}\n", std::strerror(errno));
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(0);

    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        fmt::print(stderr, "bind failed: {}\n", std::strerror(errno));
        return 1;
    }

    if (listen(fd, SOMAXCONN) != 0) {
        fmt::print(stderr, "listen failed: {}\n", std::strerror(errno));
        return 1;
    }

    fmt::print("Listening on port {}\n", PORT);

    std::vector<std::unique_ptr<Connection>> connections; // index is fd
    set_nonblocking(fd);

    std::vector<pollfd> pollfds;
    while (true) {
        pollfds.clear();

        const pollfd pfd{fd, POLLIN, 0};
        pollfds.push_back(pfd);

        for (auto &conn : connections) {
            if (conn == nullptr) {
                continue;
            }

            pollfd temp_pfd{};
            temp_pfd.fd = conn->fd;
            temp_pfd.events = (conn->state == State::REQUEST) ? POLLIN : POLLOUT;

            pollfds.push_back(temp_pfd);
        }

        const int nready =
            poll(pollfds.data(), static_cast<nfds_t>(pollfds.size()), 1000);
        if (nready < 0) {
            fmt::print(stderr, "poll failed: {}\n", std::strerror(errno));
            return 1;
        }

        for (std::size_t i = 1; i < pollfds.size(); ++i) {
            if (pollfds[i].revents != 0) {
                auto &conn = connections[pollfds[i].fd];
                connection_io(conn);
                if (conn->state == State::END) {
                    close(conn->fd);
                    connections[pollfds[i].fd].reset();
                }
            }
        }

        if (pollfds[0].revents != 0) {
            accept_new_connection(connections, fd);
        }
    }
}