#pragma once

#include "connection.hpp"

#include <memory> // std::unique_ptr

void do_unknown(std::unique_ptr<Connection> &conn);
void do_get(std::unique_ptr<Connection> &conn);
void do_set(std::unique_ptr<Connection> &conn);
void do_del(std::unique_ptr<Connection> &conn);