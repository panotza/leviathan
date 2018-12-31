/// Handling request connections.

use error::Res;
use std;
use std::{io, mem, time};
use std::io::prelude::*;
use std::os::unix::net as unet;

/// Queue of request connections, read from a socket listener.
pub struct RequestListener<'a> {
    listener: &'a unet::UnixListener,
}

impl<'a> RequestListener<'a> {
    pub fn new(listener: &'a unet::UnixListener) -> Self {
        Self {
            listener
        }
    }
}

impl<'a> Iterator for RequestListener<'a> {
    type Item = Res<RequestConnection>;

    /// Block until the next request connection arrives, then yield it.
    fn next(&mut self) -> Option<Self::Item> {
        println!("# accepting connection ...");
        let (connection, _) = match self.listener.accept() {
            Ok(p) => p,
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => return None,
            Err(e) => return Some(Err(e.into())),
        };
        const TIMEOUT_MS: u64 = 250;
        if let Err(e) = connection.set_read_timeout(
            Some(time::Duration::from_millis(TIMEOUT_MS))
        ) {
            return Some(Err(e.into()));
        };
        Some(Ok(
            RequestConnection { connection }
        ))
    }
}

/// Unread and unexecuted requests with their open socket connection.
pub struct RequestConnection {
    connection: unet::UnixStream,
}

impl RequestConnection {
    /// Read and execute these requests, closing the connection.
    pub fn execute(mut self) -> Res<()> {
        // TODO: Implement parsing and sending back proper RPC errors.
        let request_length = {
            let mut u32_buffer = [0u8; mem::size_of::<u32>()];
            let read = self.connection.read(&mut u32_buffer)
                .or_else(|e| match e.kind() {
                    io::ErrorKind::WouldBlock => Ok(0),
                    _ => Err(e),
                })?;
            if read == u32_buffer.len() {
                // TODO: Replace all this with from_be_bytes() once stable.
                let length: u32 = unsafe {
                    // NOTE: [unsafe] OK because the buffer is the correct size.
                    mem::transmute(u32_buffer)
                };
                Some(u32::from_be(length))
            } else {
                eprintln!("warn: invalid request length: only {} bytes read",
                          read);
                None
            }
        };
        let request_length = match request_length {
            Some(length) if length != 0 => length,
            Some(length) => {
                write!(self.connection, "<INVALID REQUEST LENGTH {}>",
                       length)?;
                return Ok(());
            },
            None => {
                write!(self.connection, "<INVALID REQUEST LENGTH>")?;
                return Ok(());
            },
        };
        let request_length = if request_length <= (std::usize::MAX as u32) {
            request_length as usize
        } else {
            write!(self.connection, "<REQUEST LENGTH TOO LARGE>")?;
            return Ok(());
        };
        let request = {
            let mut buffer = vec![0; request_length];
            let read = self.connection.read(&mut buffer[..])
                .or_else(|e| match e.kind() {
                    io::ErrorKind::WouldBlock => Ok(0),
                    _ => Err(e),
                })?;
            if read == buffer.len() {
                String::from_utf8(buffer)
            } else {
                write!(self.connection,
                       "<INVALID REQUEST LENGTH: read={} < buffer.len()={}>",
                       read, buffer.len())?;
                return Ok(());
            }
        };
        let request = match request {
            Ok(request) => request,
            Err(e) => {
                write!(self.connection, "<REQUEST NOT VALID UTF-8: {}>", e)?;
                return Ok(());
            },
        };
        let mut output = io::BufWriter::new(&self.connection);
        write!(output, "<response to {:?}>", request)?;
        Ok(())
    }
}
