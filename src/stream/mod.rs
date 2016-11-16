/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

mod socket;
pub use self::socket::{SocketStream,git_socket_stream_new};

#[repr(C)]
pub struct GitStream {
    pub version: i32,

    pub encrypted: i32,
    pub proxy_support: i32,
    pub connect: extern fn(st: *mut GitStream) -> i32,
    pub certificate: Option<extern fn(out: *mut *mut u8, st: *mut GitStream) -> i32>,
    pub set_proxy: Option<extern fn(st: *mut GitStream, proxy_opts: *const u8) -> i32>,
    pub read: extern fn(st: *mut GitStream, ptr: *mut u8, len: usize) -> i32,
    pub write: extern fn(st: *mut GitStream, ptr: *const u8, len: usize) -> i32,
    pub close: extern fn(st: *mut GitStream) -> i32,
    pub free: extern fn(st: *mut GitStream),
}
