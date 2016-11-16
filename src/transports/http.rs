/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

use std::{io,mem,slice};
use std::ffi::CStr;
use std::os::raw::{c_char};
use std::io::prelude::*;
use std;

use hyper;
use hyper::header::{QualityItem,qitem,UserAgent,Accept,ContentType,TransferEncoding,Encoding,ContentLength,Location};
use hyper::mime::{Mime,TopLevel,SubLevel};
use hyper::client::request::Request;
use hyper::client::response::Response;
use hyper::status::StatusCode;
use hyper::method::Method;
use hyper::Url;
use hyper::error::ParseError;
use hyper::net::{Fresh,Streaming};

use stream::SocketStream;
use transports::{GitTransport,GitSmartSubtransport,GitSmartSubtransportStream,GitSmartService};
use super::super::error::{GitError, set_error};

static UPLOAD_PACK_LS_SERVICE_QUERY: &'static str = "service=git-upload-pack";
static UPLOAD_PACK_LS_SERVICE_URL: &'static [&'static str] = &["info", "refs"];
static UPLOAD_PACK_SERVICE_URL: &'static [&'static str] = &["git-upload-pack"];


static RECEIVE_PACK_LS_SERVICE_QUERY: &'static str = "service=git-receive-pack";
static RECEIVE_PACK_LS_SERVICE_URL: &'static [&'static str] = &["info", "refs"];
static RECEIVE_PACK_SERVICE_URL: &'static [&'static str] = &["git-receive-pack"];

lazy_static! {
    static ref RECEIVE_PACK_CT_MIME: Mime = Mime(
        TopLevel::Application, SubLevel::Ext("x-git-receive-pack-request".into()), vec![]);
    static ref UPLOAD_PACK_CT_MIME: Mime = Mime(
        TopLevel::Application, SubLevel::Ext("x-git-upload-pack-request".into()), vec![]);

    static ref RECEIVE_PACK_ACCEPT: Vec<QualityItem<Mime>> = vec![
        qitem(Mime(TopLevel::Application, SubLevel::Ext("x-git-receive-pack-result".into()), vec![])),
    ];
    static ref UPLOAD_PACK_ACCEPT: Vec<QualityItem<Mime>> = vec![
        qitem(Mime(TopLevel::Application, SubLevel::Ext("x-git-upload-pack-result".into()), vec![])),
    ];
}

#[derive(PartialEq)]
pub enum HttpVerb {
    Get,
    Post,
}

#[derive(PartialEq, Clone, Copy)]
pub enum GitService {
    UploadPack,
    ReceivePack,
}

// Minimal size for writes that should trigger any IO
static CHUNK_SIZE: usize = 4096;

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

type HttpError = UrlError;

#[derive(Debug)]
pub enum Error {
    ParseError(ParseError),
    IoError(io::Error),
    UrlError(UrlError),
    HttpError(HttpError),
    HyperError(hyper::Error),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        match self {
            &Error::ParseError(ref e) => e.fmt(f),
            &Error::IoError(ref e) => e.fmt(f),
            &Error::UrlError(ref e) => e.fmt(f),
            &Error::HttpError(ref e) => e.fmt(f),
            &Error::HyperError(ref e) => e.fmt(f),
        }
    }
}

impl std::error::Error for Error {
    fn description(&self) -> &str {
        match self {
            &Error::ParseError(ref e) => e.description(),
            &Error::IoError(ref e) => e.description(),
            &Error::UrlError(ref e) => e.description(),
            &Error::HttpError(ref e) => e.description(),
            &Error::HyperError(ref e) => e.description(),
        }
    }

    fn cause(&self) -> Option<&std::error::Error> {
        match self {
            &Error::ParseError(ref e) => e.cause(),
            &Error::IoError(ref e) => e.cause(),
            &Error::UrlError(ref e) => e.cause(),
            &Error::HttpError(ref e) => e.cause(),
            &Error::HyperError(ref e) => e.cause(),
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

impl std::convert::From<hyper::Error> for Error {
    fn from(e: hyper::Error) -> Self {
        Error::HyperError(e)
    }
}

#[repr(C)]
pub struct Http {
    parent: GitSmartSubtransport,
    owner: *mut GitTransport,
    url: Option<Url>,
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
            connected: false,
        })
    }

    fn parse_url(&mut self, url: &str) -> Result<(), Error> {
        println!("url => {}", url);
        let parsed = try!(Url::parse(url));
        if parsed.host_str().is_none() {
            return Err(Error::UrlError(UrlError{error: "there is no host in the url"}))
        }
        if parsed.port_or_known_default().is_none() {
            return Err(Error::UrlError(UrlError{error: "there is no port for this url"}))
        }

        if parsed.scheme() != "http" {
            println!(":(");
            return Err(Error::UrlError(UrlError{error: "sorry no HTTPS"}));
        }

        self.url = Some(parsed);
        Ok(())
    }

    pub fn connect(&mut self) -> Result<(), Error> {
        // TODO: need to check if we have keep-alive when we're already connected

        self.connected = false;
        Ok(())
    }

    pub fn action(&mut self, url: &str, action: GitSmartService) -> Result<Box<HttpStream>,Error> {
        if self.url.is_none() {
            try!(self.parse_url(url));
        }
        try!(self.connect());

        let stream = match action {
            GitSmartService::UploadPackLs =>
                HttpStream::new(self, GitService::UploadPack, UPLOAD_PACK_LS_SERVICE_URL, Some(UPLOAD_PACK_LS_SERVICE_QUERY), HttpVerb::Get, false),
            GitSmartService::UploadPack =>
                HttpStream::new(self, GitService::UploadPack, UPLOAD_PACK_SERVICE_URL, None, HttpVerb::Post, false),
            GitSmartService::ReceivePackLs =>
                HttpStream::new(self, GitService::ReceivePack, RECEIVE_PACK_LS_SERVICE_URL, Some(RECEIVE_PACK_LS_SERVICE_QUERY), HttpVerb::Get, false),
            GitSmartService::ReceivePack =>
                HttpStream::new(self, GitService::ReceivePack, RECEIVE_PACK_SERVICE_URL, None, HttpVerb::Post, true),
        };

        Ok(stream)
    }

    pub fn close(&mut self) {
    }
}

#[repr(C)]
pub struct HttpStream {
    parent: GitSmartSubtransportStream,
    service: GitService,
    service_url: &'static [&'static str],
    url_query: Option<&'static str>,
    verb: HttpVerb,
    chunk_buffer: Vec<u8>,
    chunked: bool,
    sent_request: bool,
    received_response: bool,
    header_finished: bool,
    request: Option<Request<Streaming>>,
    response: Option<Response>,
}

impl HttpStream {
    pub fn new(transport: &mut Http, service: GitService, service_url: &'static [&'static str], url_query: Option<&'static str>, verb: HttpVerb, chunked: bool) -> Box<Self> {
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
            url_query: url_query,
            verb: verb,
            chunk_buffer: Vec::with_capacity(CHUNK_SIZE),
            chunked: chunked,
            sent_request: false,
            received_response: false,
            header_finished: false,
            request: None,
            response: None,
        })
    }

    fn response_or_send(&mut self, http: &mut Http) -> Result<&mut Response, Error> {
        match self.response {
            Some(ref mut resp) => Ok(resp),
            None => {
                let sreq = match self.request.take() {
                    Some(sreq) => sreq,
                    None => {
                        let req = self.gen_request(http.url.as_ref().unwrap(), None);
                        try!(req.start())
                    }
                };
                let resp = try!(sreq.send());
                if resp.status.is_redirection() {
                    let location = try!(resp.headers.get::<Location>()
                                        .ok_or(Error::HttpError(HttpError{error: "Redirect without a location"})));
                    println!("redirect to {:?}", location);
                    try!(http.parse_url(location));
                    // FIXME: this should loop instead
                    return self.response_or_send(http)
                }

                if resp.status != StatusCode::Ok {
                    println!("status {:?}", resp.status);
                    return Err(Error::HttpError(HttpError{error: "Did not get 200"}));
                }

                self.response = Some(resp);
                Ok(self.response.as_mut().unwrap())
            }
        }
    }

    pub fn read(&mut self, buffer: &mut [u8]) -> Result<usize, Error> {
        let http: &mut Http = unsafe { mem::transmute(self.parent.subtransport ) };
        let mut response = try!(self.response_or_send(http));

        // ugly way to invoking the conversion
        Ok(try!(response.read(buffer)))
    }

    pub fn write(&mut self, data: &[u8]) -> Result<(), Error> {
        let mut http: &mut Http = unsafe { mem::transmute(self.parent.subtransport ) };
        assert!(http.connected);

        if self.chunked {
            self.write_chunked(&mut http, data)
        } else {
            self.write_single(&mut http, data)
        }
    }

    fn write_single(&mut self, http: &mut Http, data: &[u8]) -> Result<(), Error> {
        let req = self.gen_request(http.url.as_ref().unwrap(), None);
        let mut sreq = try!(req.start());
        try!(sreq.write_all(data));
        try!(sreq.send());
        Ok(())
    }

    fn write_chunked(&mut self, http: &mut Http, data: &[u8]) -> Result<(), Error> {
        let mut sreq = match self.request {
            Some(ref mut sreq) => sreq,
            None => {
                let req = self.gen_request(http.url.as_ref().unwrap(), None);
                let sreq = try!(req.start());
                self.request = Some(sreq);
                self.request.as_mut().unwrap()
            }
        };

        match sreq.write_all(data) {
            Ok(_) => Ok(()),
            Err(e) => Err(From::from(e)),
        }
    }

    fn gen_request(&self, url: &Url, data: Option<&[u8]>) -> Request<Fresh> {
        let mut service_url = url.clone();
        service_url.path_segments_mut().unwrap().extend(self.service_url);
        service_url.set_query(self.url_query);

        let mut req = match self.verb {
            HttpVerb::Get => Request::new(Method::Get, service_url).unwrap(),
            HttpVerb::Post => Request::new(Method::Post, service_url).unwrap(),
        };

        {
            let mut headers = req.headers_mut();
            // TODO: support querying the user-agent from the user
            headers.set(UserAgent("git/1.0 (libgit2core)".into()));
            if self.chunked || data.map_or(0, |d| d.len()) > 0 {
                headers.set(Accept(accept_for_service(self.service)));
                headers.set(ContentType(content_type_for_service(self.service)));
                if self.chunked {
                    headers.set(TransferEncoding(vec![Encoding::Chunked]))
                } else {
                    headers.set(ContentLength(data.unwrap().len() as u64));
                }
            }
            // TODO: support custom header fields
        }

        req
    }
}

fn content_type_for_service(service: GitService) -> Mime {
    match service {
        GitService::UploadPack => UPLOAD_PACK_CT_MIME.clone(),
        GitService::ReceivePack => RECEIVE_PACK_CT_MIME.clone(),
    }
}

fn accept_for_service(service: GitService) -> Vec<QualityItem<Mime>> {
    match service {
        GitService::UploadPack => UPLOAD_PACK_ACCEPT.clone(),
        GitService::ReceivePack => RECEIVE_PACK_ACCEPT.clone(),
    }
}

// And here is the code to bind it all to C

extern fn http_stream_read(stream: *mut GitSmartSubtransportStream, buffer: *mut u8, len: usize, bytes_read: *mut usize) -> i32 {
    unsafe {
        let mut stream: &mut HttpStream = mem::transmute(stream);
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
        let mut stream: &mut HttpStream = mem::transmute(stream);
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
    let _stream: &mut HttpStream = unsafe { mem::transmute(stream) };
}

extern fn http_close(transport: *mut GitSmartSubtransport) -> i32 {
    let mut t: &mut Http = unsafe { mem::transmute(transport) };
    t.close();
    0
}

extern fn http_free(transport: *mut GitSmartSubtransport) {
    unsafe { Box::from_raw(transport) };
}

extern fn http_action(out: *mut *mut GitSmartSubtransportStream, transport: *mut GitSmartSubtransport, url: *const c_char, action: GitSmartService) -> i32 {
    let mut t: &mut Http = unsafe { mem::transmute(transport) };
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
    println!("Hello from rust");

    *out = mem::transmute(Box::into_raw(t));
    0
}
