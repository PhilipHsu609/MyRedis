# MyRedis

A simple implementation of [Redis](https://redis.io/) in C++.

This project is a part of my learning process of C++ and Redis from [build-your-own-redis](https://build-your-own.org/redis/).

## Protocol

Byte-based protocol. Assume all integers are in little-endian.

```text
| len | content |
```

- `len` is a 4-byte integer representing the length of `content`.
- `content` can be either a request or a response.

### Request

```text
| nstr | str 1 len | str 1 | str 2 len | str 2 | ... | str n len | str n |
```

- `nstr` is a 4-byte integer representing the number of strings.
- `str 1` must be a command, the rest are arguments.

### Response

```text
| status | message |
```

- `status` is a 1-byte integer representing the status of the response.
- `message` is a string.

## Status

- [x] Basic client-server communication
- [x] Basic commands, GET, SET and DEL
