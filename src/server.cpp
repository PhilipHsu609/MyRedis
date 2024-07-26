#include "connection.hpp"
#include "hashtable.hpp"
#include "utils.hpp"

#include <fmt/ranges.h> // fmt::format

#include <array>   // std::array
#include <cerrno>  // errno
#include <cstddef> // std::size_t
#include <cstdlib> // EXIT_FAILURE
#include <cstring> // std::strerror, std::memcpy, std::memmove
#include <memory>  // std::unique_ptr
#include <vector>  // std::vector

#include <netinet/in.h> // sockaddr_in
#include <sys/epoll.h>  // epoll_event, epoll_create1, epoll_ctl
#include <sys/socket.h> // accept4, bind, listen, socket, sockaddr
#include <sys/types.h>  // ssize_t
#include <unistd.h>     // close, read, write, socklen_t

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
HashTable map;

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
        const std::size_t remain = conn->wbuf_size - conn->wbuf_pos;
        n = write(conn->fd, &conn->wbuf[conn->wbuf_pos], remain);
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

    conn->wbuf_pos += n;

    if (conn->wbuf_pos == conn->wbuf_size) {
        // Response was fully sent
        conn->wbuf_size = 0;
        conn->wbuf_pos = 0;
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
    // Process the request
    const ReqStatus status = do_request(conn);

    if (status == ReqStatus::AGAIN) {
        return false;
    }

    if (status == ReqStatus::ERR) {
        conn->state = ConnState::END;
        return false;
    }

    return conn->state == ConnState::REQUEST;
}

bool try_fill_buffer(std::unique_ptr<Connection> &conn) {
    auto &rbuf = conn->rbuf;

    if (conn->rbuf_pos > 0) {
        // Remove handled requests from the buffer
        const std::size_t remain = conn->rbuf_size - conn->rbuf_pos;
        std::memmove(rbuf.data(), &rbuf[conn->rbuf_pos], remain);
        conn->rbuf_size = remain;
        conn->rbuf_pos = 0;
    }

    ssize_t n = 0;

    do {
        // Read as much data as possible, up to the buffer size
        const std::size_t remain = conn->rbuf.size() - conn->rbuf_size;
        n = read(conn->fd, &rbuf[conn->rbuf_size], remain);
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