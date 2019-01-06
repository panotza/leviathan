/// Handling request connections.

use error::Res;
use jrpc::futures::Future;
use jsonrpc_core as jrpc;
use serde_json;
use serde::{Deserialize, Serialize};
use std::{io, sync, time};
use std::io::prelude::*;
use std::os::unix::net as unet;

use JRPC_VERSION;

/// Queue of request connections, read from a socket listener.
pub struct RequestListener<'h, 'l> {
    listener: &'l unet::UnixListener,
    rpc_handler: &'h sync::RwLock<jrpc::IoHandler>,
}

impl<'h, 'l> RequestListener<'h, 'l> {
    pub fn new(listener: &'l unet::UnixListener,
               rpc_handler: &'h sync::RwLock<jrpc::IoHandler>) -> Self {
        Self {
            listener,
            rpc_handler,
        }
    }
}

impl<'h, 'l> Iterator for RequestListener<'h, 'l> {
    type Item = Res<RequestConnection<'h>>;

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
        println!("# established connection");
        Some(Ok(
            RequestConnection {
                connection,
                rpc_handler: self.rpc_handler,
            }
        ))
    }
}

/// Unread and unexecuted request(s) with its open socket connection.
pub struct RequestConnection<'h> {
    connection: unet::UnixStream,
    rpc_handler: &'h sync::RwLock<jrpc::IoHandler>,
}

impl<'h> RequestConnection<'h> {
    /// Read and execute this request, closing the connection.
    pub fn execute(mut self) -> Res<()> {
        let request = match self.read_request()? {
            Ok(request) => request,
            Err(error) => {
                eprintln!("warn: cannot read request: {:?}", error);
                let response = jrpc::Response::from(error, JRPC_VERSION);
                self.write_response(&response)?;
                return Ok(());
            },
        };

        let response = {
            // NOTE: [unwrap] Forward panic.
            let rpc_handler = self.rpc_handler.read().unwrap();
            // NOTE: [expect] This should never happen.
            rpc_handler.handle_rpc_request(request).wait()
                .expect("handler call error")
        };
        match response {
            Some(response) => {
                println!("# sending response to method call");
                self.write_response(&response)
            },
            None => {
                println!("# sending no response to notification");
                Ok(())
            },
        }
    }

    fn read_request(&mut self) -> Res<Result<jrpc::Request, jrpc::Error>> {
        fn error_serde_to_jrpc(e: serde_json::Error) -> jrpc::Error {
            let code = match e.classify() {
                serde_json::error::Category::Data => {
                    jrpc::ErrorCode::InvalidRequest
                },
                serde_json::error::Category::Eof |
                serde_json::error::Category::Io |
                serde_json::error::Category::Syntax => {
                    jrpc::ErrorCode::ParseError
                },
            };
            let message = code.description();
            let data = Some(format!("{}", e).into());
            jrpc::Error { code, message, data }
        }

        let mut reader = io::BufReader::new(&mut self.connection);
        let mut de = serde_json::Deserializer::from_reader(&mut reader);
        Ok(
            jrpc::Request::deserialize(&mut de).map_err(error_serde_to_jrpc)
        )
    }

    fn write_response(&self, response: &jrpc::Response) -> Res<()> {
        let mut writer = io::BufWriter::new(&self.connection);
        {
            let mut ser = serde_json::Serializer::new(&mut writer);
            // NOTE: [expect] This should never happen
            response.serialize(&mut ser)
                .expect("response serialization error");
        }
        // NOTE: Explicit flush because the destructor does not check for
        // errors.
        writer.flush()?;
        Ok(())
    }
}
