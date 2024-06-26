#include "command.hpp"
#include "utils.hpp"

#include <fmt/ranges.h> // fmt::print

#include <array>       // std::array
#include <cerrno>      // errno
#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint8_t
#include <cstdio>      // stderr, BUFSIZ
#include <cstdlib>     // EXIT_FAILURE
#include <cstring>     // std::strerror, std::memcpy, std::memmove
#include <memory>      // std::unique_ptr
#include <string>      // std::string
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <netinet/in.h> // sockaddr_in
#include <sys/epoll.h>  // epoll_event, epoll_create1, epoll_ctl
#include <sys/socket.h> // accept4, bind, listen, socket, sockaddr
#include <sys/types.h>  // ssize_t
#include <unistd.h>     // close, read, write, socklen_t

enum class ConnState : std::uint8_t { REQUEST = 0, RESPONSE = 1, END = 2 };

struct Connection {
    int fd = -1;
    // Current state of the connection
    ConnState state = ConnState::REQUEST;
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

    const int client_fd = accept4(listen_fd, reinterpret_cast<sockaddr *>(&addr),
                                  &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd == -1) {
        LOG_ERROR(fmt::format("accept failed: {}", std::strerror(errno)));
        return -1;
    }

    add_connection(connections, client_fd);

    return client_fd;
}

bool try_flush_buffer(std::unique_ptr<Connection> &conn) {
    ssize_t n = 0;

    do {
        // Write as much data as possible, up to the buffer size
        const std::size_t remain = conn->wbuf_size - conn->wbuf_sent;
        n = write(conn->fd, conn->wbuf.data() + conn->wbuf_sent, remain);
    } while (n == -1 && errno == EINTR);

    if (n == -1 && errno == EAGAIN) {
        // Resource temporarily unavailable, try again later
        return false;
    }

    if (n == -1) {
        LOG_ERROR(fmt::format("write failed: {}", std::strerror(errno)));
        conn->state = ConnState::END;
        return false;
    }

    conn->wbuf_sent += n;

    if (conn->wbuf_sent == conn->wbuf_size) {
        // Response was fully sent
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        conn->state = ConnState::REQUEST;
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
        LOG_ERROR(fmt::format("Invalid length: {}", len));
        conn->state = ConnState::END;
        return false;
    }

    const std::size_t r_offset = conn->read_size + COMMAND_SIZE;
    if (conn->rbuf_size < r_offset + len) {
        // Not enough data in the buffer
        return false;
    }

    LOG_INFO(fmt::format("Received: fd = {}, len = {}", conn->fd, len));

    // Process the request
    const Response res = do_request(conn->rbuf.data() + r_offset, len,
                                    // Skip some bytes for the response length and status
                                    conn->wbuf.data() + COMMAND_SIZE + sizeof(ResStatus));
    conn->read_size += COMMAND_SIZE + len; // Request had been processed

    if (res.status == ResStatus::ERR) {
        LOG_ERROR("Process request failed, possibly wrong format");
        conn->state = ConnState::END;
        return false;
    }

    // Flush the write buffer if it's full
    if (conn->wbuf_size + COMMAND_SIZE + sizeof(ResStatus) + res.len >
        conn->wbuf.size()) {
        conn->state = ConnState::RESPONSE;
        state_res(conn);
    }

    len = sizeof(ResStatus) + res.len; // Response status + data length
    std::memcpy(conn->wbuf.data() + conn->wbuf_size, &len, COMMAND_SIZE);
    std::memcpy(conn->wbuf.data() + conn->wbuf_size + COMMAND_SIZE, &res.status,
                sizeof(ResStatus));
    conn->wbuf_size += COMMAND_SIZE + len;

    return conn->state == ConnState::REQUEST;
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
        if (n == -1) {
            // Error
            LOG_ERROR(fmt::format("read failed: {}", std::strerror(errno)));
        } else {
            // EOF
            LOG_INFO(fmt::format("Connection closed: fd = {}", conn->fd));
        }
        conn->state = ConnState::END;
        return false;
    }

    conn->rbuf_size += n;

    // Process requests one by one
    while (try_one_request(conn)) {
    }

    if (conn->state == ConnState::END) {
        LOG_ERROR(fmt::format("Connection closed without respond: fd = {}", conn->fd));
        return false;
    }

    // Change the state to response
    conn->state = ConnState::RESPONSE;
    state_res(conn);

    return conn->state == ConnState::REQUEST;
}

void state_req(std::unique_ptr<Connection> &conn) {
    while (try_fill_buffer(conn)) {
    }
}

void connection_io(std::unique_ptr<Connection> &conn) {
    if (conn->state == ConnState::REQUEST) {
        state_req(conn);
    } else if (conn->state == ConnState::RESPONSE) {
        state_res(conn);
    }
}

int main() {
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        LOG_ERROR(fmt::format("socket failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    int val = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        LOG_ERROR(fmt::format("setsockopt failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(0);

    if (bind(listen_fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1) {
        LOG_ERROR(fmt::format("bind failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        LOG_ERROR(fmt::format("listen failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    LOG_INFO(fmt::format("Listening on port {}", PORT));

    set_nonblocking(listen_fd);
    std::vector<std::unique_ptr<Connection>> connections; // index is fd
    std::array<epoll_event, MAX_EVENTS> events{};

    const int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        LOG_ERROR(fmt::format("epoll_create1 failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;

    // Add the listening socket to the epoll set
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        LOG_ERROR(fmt::format("epoll_ctl failed: {}", std::strerror(errno)));
        return EXIT_FAILURE;
    }

    while (true) {
        const int nready = epoll_wait(epfd, events.data(), MAX_EVENTS, -1);
        if (nready < 0) {
            LOG_ERROR(fmt::format("epoll_wait failed: {}", std::strerror(errno)));
            return EXIT_FAILURE;
        }

        for (int i = 0; i < nready; ++i) {
            if (events[i].data.fd == listen_fd) {
                const int client_fd = accept_new_connection(connections, listen_fd);
                LOG_INFO(fmt::format("Accepted new connection: fd = {}", client_fd));

                // Add the new connection to the epoll set
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    LOG_ERROR(fmt::format("epoll_ctl failed: {}", std::strerror(errno)));
                    return EXIT_FAILURE;
                }
            } else {
                auto &conn = connections[events[i].data.fd];
                connection_io(conn);
                if (conn->state == ConnState::END) {
                    close(conn->fd);
                    conn.reset();
                }
            }
        }
    }
}