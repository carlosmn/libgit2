/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

use std::{io,mem,slice};
use std::ffi::CStr;
use std::os::raw::{c_char};
use std;

use url::{Url,ParseError};

use stream::SocketStream;
use transports::{GitTransport,GitSmartSubtransport,GitSmartSubtransportStream,GitSmartService};
use super::super::error::{GitError, set_error};

static UPLOAD_PACK_SERVICE: &'static str = "upload-pack";
static UPLOAD_PACK_LS_SERVICE_URL: &'static str = "/info/refs?service=git-upload-pack";
static UPLOAD_PACK_SERVICE_URL: &'static str = "/git-upload-pack";


static RECEIVE_PACK_SERVICE: &'static str = "receive-pack";
static RECEIVE_PACK_LS_SERVICE_URL: &'static str = "/info/refs?service=git-receive-pack";
static RECEIVE_PACK_SERVICE_URL: &'static str = "/git-receive-pack";

static VERB_GET:  &'static str = "GET";
static VERB_POST: &'static str = "POST";

#[derive(Debug)]
pub struct UrlError {
    error: &'static str,
}

impl std::fmt::Display for UrlError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        write!(f, "{}", self.error)
    }
}

impl std::error::Error for UrlError {
    fn description(&self) -> &str {
        self.error
    }

    fn cause(&self) -> Option<&std::error::Error> {
        None
    }
}

#[derive(Debug)]
pub enum Error {
    ParseError(ParseError),
    IoError(io::Error),
    UrlError(UrlError)
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        match self {
            &Error::ParseError(ref e) => e.fmt(f),
            &Error::IoError(ref e) => e.fmt(f),
            &Error::UrlError(ref e) => e.fmt(f),
        }
    }
}

impl std::error::Error for Error {
    fn description(&self) -> &str {
        match self {
            &Error::ParseError(ref e) => e.description(),
            &Error::IoError(ref e) => e.description(),
            &Error::UrlError(ref e) => e.description(),
        }
    }

    fn cause(&self) -> Option<&std::error::Error> {
        match self {
            &Error::ParseError(ref e) => e.cause(),
            &Error::IoError(ref e) => e.cause(),
            &Error::UrlError(ref e) => e.cause(),
        }
    }
}

impl std::convert::From<ParseError> for Error {
    fn from(e: ParseError) -> Self {
        Error::ParseError(e)
    }
}

impl std::convert::From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error::IoError(e)
    }
}

#[repr(C)]
pub struct Http {
    parent: GitSmartSubtransport,
    owner: *mut GitTransport,
    url: Option<Url>,
    io: Option<Box<SocketStream>>,
    connected: bool
}

impl Http {
    pub fn new(owner: *mut GitTransport) -> Box<Self> {
        Box::new(Http {
            parent: GitSmartSubtransport {
                action: http_action,
                close: http_close,
                free: http_free,
            },
            owner: owner,
            url: None,
            io: None,
            connected: false,
        })
    }

    fn parse_url(&mut self, url: &str) -> Result<(), Error> {
        let parsed = try!(Url::parse(url));
        if parsed.host_str().is_none() {
            return Err(Error::UrlError(UrlError{error: "there is no host in the url"}))
        }
        if parsed.port_or_known_default().is_none() {
            return Err(Error::UrlError(UrlError{error: "there is no port for this url"}))
        }

        Ok(())
    }

    pub fn connect(&mut self) -> Result<(), Error> {
        // TODO: need to check if we have keep-alive when we're already connected

        if let Some(mut io) = self.io.take() {
            io.close();
        }
        self.connected = false;

        let url = self.url.as_ref().unwrap();
        let mut io = SocketStream::new(url.host_str().unwrap(), url.port_or_known_default().unwrap());
        try!(io.connect());

        self.io = Some(io);
        Ok(())
    }

    pub fn action(&mut self, url: &str, action: GitSmartService) -> Result<Box<HttpStream>,Error> {
        if self.url.is_none() {
            try!(self.parse_url(url));
        }

        let stream = match action {
            GitSmartService::UploadPackLs =>
                HttpStream::new(self, UPLOAD_PACK_SERVICE, UPLOAD_PACK_LS_SERVICE_URL, VERB_GET, false),
            GitSmartService::UploadPack =>
                HttpStream::new(self, UPLOAD_PACK_SERVICE, UPLOAD_PACK_SERVICE_URL, VERB_POST, false),
            GitSmartService::ReceivePackLs =>
                HttpStream::new(self, RECEIVE_PACK_SERVICE, RECEIVE_PACK_LS_SERVICE_URL, VERB_GET, false),
            GitSmartService::ReceivePack =>
                HttpStream::new(self, RECEIVE_PACK_SERVICE, RECEIVE_PACK_SERVICE_URL, VERB_POST, true),
        };

        Ok(stream)
    }

    pub fn close(&mut self) {
        if let Some(mut io) = self.io.take() {
            io.close();
        }
    }
}

#[repr(C)]
pub struct HttpStream {
    parent: GitSmartSubtransportStream,
    service: &'static str,
    service_url: &'static str,
    verb: &'static str,
    chunked: bool,
}

impl HttpStream {
    pub fn new(transport: &mut Http, service: &'static str, service_url: &'static str, verb: &'static str, chunked: bool) -> Box<Self> {
        let parent: &mut GitSmartSubtransport = &mut transport.parent;
        Box::new(HttpStream {
            parent: GitSmartSubtransportStream {
                subtransport: parent as *mut GitSmartSubtransport,
                read: http_stream_read,
                write: http_stream_write,
                free: http_stream_free,
            },
            service: service,
            service_url: service_url,
            verb: verb,
            chunked: chunked,
        })
    }

    pub fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        let http: Box<Http> = unsafe { mem::transmute(self.parent.subtransport ) };
        if let Some(mut io) = http.io {
            io.read(buffer)
        } else {
            unimplemented!()
        }
    }

    pub fn write(&mut self, data: &[u8]) -> io::Result<()> {
        let http: Box<Http> = unsafe { mem::transmute(self.parent.subtransport ) };
        if let Some(mut io) = http.io {
            io.write(data)
        } else {
            unimplemented!()
        }
    }
}


// And here is the code to bind it all to C

extern fn http_stream_read(stream: *mut GitSmartSubtransportStream, buffer: *mut u8, len: usize, bytes_read: *mut usize) -> i32 {
    unsafe {
        let mut stream: Box<HttpStream> = mem::transmute(stream);
        let data = slice::from_raw_parts_mut(buffer, len);
        match stream.read(data) {
            Ok(n) => {
                *bytes_read = n;
                0
            },
            Err(e) => {
                set_error(GitError::Net, e);
                -1
            }
        }
    }
}

extern fn http_stream_write(stream: *mut GitSmartSubtransportStream, buffer: *mut u8, len: usize) -> i32 {
    unsafe {
        let mut stream: Box<HttpStream> = mem::transmute(stream);
        let data = slice::from_raw_parts(buffer, len);
        match stream.write(data) {
            Ok(_) => 0,
            Err(e) => {
                set_error(GitError::Net, e);
                -1
            }
        }
    }
}

extern fn http_stream_free(stream: *mut GitSmartSubtransportStream) {
    let _stream: Box<HttpStream> = unsafe { Box::from_raw(mem::transmute(stream)) };
}

extern fn http_close(transport: *mut GitSmartSubtransport) -> i32 {
    let mut t: Box<Http> = unsafe { mem::transmute(transport) };
    t.close();
    0
}

extern fn http_free(transport: *mut GitSmartSubtransport) {
    unsafe { Box::from_raw(transport) };
}

extern fn http_action(out: *mut *mut GitSmartSubtransportStream, transport: *mut GitSmartSubtransport, url: *const c_char, action: GitSmartService) -> i32 {
    let mut t: Box<Http> = unsafe { mem::transmute(transport) };
    let rurl = unsafe { CStr::from_ptr(url).to_str().unwrap() };

    unsafe {
        match t.action(rurl, action) {
            Ok(stream) => {
                *out = mem::transmute(Box::into_raw(stream));
                0
            },
            Err(e) => {
                set_error(GitError::Net, e);
                -1
            }
        }
    }
}

#[no_mangle]
pub unsafe extern fn git_smart_subtransport_http2(out: *mut *mut GitSmartSubtransport, owner: *mut GitTransport, _param: *mut u8) -> i32 {
    let t = Http::new(owner);

    *out = mem::transmute(Box::into_raw(t));
    0
}
