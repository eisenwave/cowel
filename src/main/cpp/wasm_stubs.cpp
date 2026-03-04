// Necessary to prevent bogus wasi_snapshot_preview1 imports,
// at least on debug builds, where they're not optimized out for some reason.
// See https://github.com/emscripten-core/emscripten/issues/17331

// NOLINTBEGIN
extern "C" [[gnu::used]]
int __wasi_fd_close(int)
{
    __builtin_trap();
}

extern "C" [[gnu::used]]
int __wasi_fd_write(int, int, int, int)
{
    __builtin_trap();
}

extern "C" [[gnu::used]]
int __wasi_fd_seek(int, long long, int, int)
{
    __builtin_trap();
}

extern "C" [[gnu::used]]
int __wasi_environ_sizes_get(int* environ_count, int* environ_buf_size)
{
    *environ_count = 0;
    *environ_buf_size = 0;
    return 0;
}

extern "C" [[gnu::used]]
int __wasi_environ_get(char**, char*)
{
    return 0;
}
// NOLINTEND
