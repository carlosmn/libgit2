mod socket;
pub use self::socket::{SocketStream,git_socket_stream_new};

#[repr(C)]
pub struct GitStream {
    version: i32,

    encrypted: i32,
    proxy_support: i32,
    connect: extern fn(st: *mut GitStream) -> i32,
    certificate: Option<extern fn(out: *mut *mut u8, st: *mut GitStream) -> i32>,
    set_proxy: Option<extern fn(st: *mut GitStream, proxy_opts: *const u8) -> i32>,
    read: extern fn(st: *mut GitStream, ptr: *mut u8, len: usize) -> i32,
    write: extern fn(st: *mut GitStream, ptr: *const u8, len: usize) -> i32,
    close: extern fn(st: *mut GitStream) -> i32,
    free: extern fn(st: *mut GitStream),
}
