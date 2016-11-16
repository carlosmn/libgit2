/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

use std::io::prelude::*;
use std::net::{TcpStream};
use std::{io,mem,slice};
use std::ffi::CStr;
use std::os::raw::{c_char};

use stream::GitStream;
use super::super::error::{GitError, set_error};

#[repr(C)]
pub struct SocketStream {
    parent: GitStream,
    stream: Option<TcpStream>,
    host: String,
    port: u16,
}

impl SocketStream {
    pub fn new(host: &str, port: u16) -> Box<Self> {
        Box::new(SocketStream{
            parent: GitStream {
                version: 1,
                encrypted: 0,
                proxy_support: 0,
                connect: socket_connect,
                certificate: None,
                set_proxy: None,
                read: socket_read,
                write: socket_write,
                close: socket_close,
                free: socket_free,
            },
            stream: None,
            host: host.into(),
            port: port,
        })
    }

    pub fn connect(&mut self) -> io::Result<()> {
        self.stream = Some(try!(TcpStream::connect((self.host.as_str(), self.port))));

        Ok(())
    }

    pub fn close(&mut self) {
        self.stream.take();
    }

    pub fn write(&mut self, data: &[u8]) -> io::Result<()> {
        self.stream.as_ref().unwrap().write_all(data)
    }

    pub fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.stream.as_ref().unwrap().read(buf)
    }

    pub fn free(&mut self) {
        drop(self)
    }
}

impl io::Read for SocketStream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.read(buf)
    }
}


// And here comes the code to make it accessible for C

extern fn socket_connect(st: *mut GitStream) -> i32 {
    let stream: &mut SocketStream = unsafe { mem::transmute(&mut *st)};
    match stream.connect() {
        Ok(_) => 0,
        Err(e) => {
            set_error(GitError::Net, e);
            -1
        },
    }
}

extern fn socket_read(st: *mut GitStream, ptr: *mut u8, len: usize) -> i32 {
    let stream: &mut SocketStream = unsafe { mem::transmute(&mut *st)};
    let mut data = unsafe { slice::from_raw_parts_mut(ptr, len) };
    match stream.read(data) {
        Ok(n) => n as i32,
        Err(e) => {
            set_error(GitError::Net, e);
            -1
        },
    }
}

extern fn socket_write(st: *mut GitStream, ptr: *const u8, len: usize) -> i32 {
    let stream: &mut SocketStream = unsafe { mem::transmute(&mut *st)};
    let data = unsafe { slice::from_raw_parts(ptr, len) };
    match stream.write(data) {
        Ok(_) => len as i32,
        Err(e) => {
            set_error(GitError::Net, e);
            -1
        },
    }
}

extern fn socket_close(st: *mut GitStream) -> i32 {
    let stream: &mut SocketStream = unsafe { mem::transmute(&mut *st)};
    stream.close();
    0
}

extern fn socket_free(st: *mut GitStream) {
    let stream: &mut SocketStream = unsafe { mem::transmute(&mut *st)};
    stream.free();
}

#[no_mangle]
pub unsafe extern fn git_socket_stream_new(out: *mut *mut GitStream, host: *const c_char, port: *const c_char) -> i32 {
    let portn = match CStr::from_ptr(port).to_str().unwrap().parse() {
        Ok(n) => n,
        Err(e) => {
            set_error(GitError::Net, e);
            return -1;
        }
    };

    let stream = SocketStream::new(CStr::from_ptr(host).to_str().unwrap(), portn);
    *out =  mem::transmute(Box::into_raw(stream));
    0
}
