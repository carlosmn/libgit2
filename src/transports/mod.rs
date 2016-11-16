/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

pub mod http;

use std::os::raw::{c_char};

pub struct GitTransport;

#[repr(i32)]
pub enum GitSmartService {
    UploadPackLs  = 1,
    UploadPack    = 2,
    ReceivePackLs = 3,
    ReceivePack   = 4,
}

pub struct GitSmartSubtransportStream {
    pub subtransport: *mut GitSmartSubtransport,
    pub read: extern fn(stream: *mut GitSmartSubtransportStream, buffer: *mut u8, len: usize, read: *mut usize) -> i32,
    pub write: extern fn(stream: *mut GitSmartSubtransportStream, buffer: *mut u8, len: usize) -> i32,
    pub free: extern fn(stream: *mut GitSmartSubtransportStream),
}

pub struct GitSmartSubtransport {
    pub action: extern fn(out: *mut *mut GitSmartSubtransportStream, transport: *mut GitSmartSubtransport, url: *const c_char, action: GitSmartService) -> i32,
    pub close: extern fn(transport: *mut GitSmartSubtransport) -> i32,
    pub free: extern fn(transport: *mut GitSmartSubtransport),
}
