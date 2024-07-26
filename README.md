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
| content |
```

- `content` is a serialized object.

### Object Serialization

```text
| type | len | content |
```

- `type` is a 1-byte character representing the type of the content.
  - It can be either array, string ,integer or null.
- `len` is a 4-byte integer representing the length of `content`.
  - If the type is array, it should be the number of objects.
  - If the type is integer, it should be the minimum number of bytes to represent the integer.
  - If the type is null, it should be 0.
- `content` is the actual content.
  - If the type is array, it should be a list of objects.
  - If the type is null, it should be empty.

## Status

- [x] Basic client-server communication
- [x] Basic commands, GET, SET and DEL
