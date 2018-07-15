use error::{Res};
use std::{ffi, fs, io, path};
use std::io::prelude::*;
use std::os::unix::net as unet;
use yaml;

pub struct Requests<'a> {
    listener: &'a unet::UnixListener,
}

impl<'a> Requests<'a> {
    pub fn new(listener: &'a unet::UnixListener) -> Self {
        Self {
            listener
        }
    }
}

impl<'a> Iterator for Requests<'a> {
    type Item = Res<Request>;

    /// Block until the next request arrives, then parse it.
    fn next(&mut self) -> Option<Self::Item> {
        println!("# accepting connection ...");
        let (connection, _) = match self.listener.accept() {
            Ok(p) => p,
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => return None,
            Err(e) => return Some(Err(e.into())),
        };
        let inner = match RequestInner::receive(connection) {
            Ok(inner) => inner,
            Err(e) => return Some(Err(e)),
        };
        Some(Ok(Request { inner }))
    }
}

pub struct Request {
    inner: Box<RequestInner>,
}

impl Request {
    pub fn execute(&mut self) -> Res<()> {
        self.inner
            .execute()?
            .send(self.inner.connection())
    }
}

trait RequestInner {
    fn connection(&mut self) -> &mut unet::UnixStream;

    fn execute(&self) -> Res<Response>;
}

impl RequestInner {
    const DRIVERS_DIR: &'static str = "/sys/bus/usb/drivers";

    const YAML_DOCUMENT_END_MARKER: &'static str = "...";

    fn receive(connection: unet::UnixStream) -> Res<Box<Self>> {
        println!("# parsing into request ...");
        let contents = {
            let mut contents = String::new();
            for line in io::BufReader::new(&connection).lines() {
                let line = line?;
                if line.starts_with(Self::YAML_DOCUMENT_END_MARKER) {
                    break;
                } else {
                    contents.push_str(&line);
                }
            }
            contents
        };
        let mut documents = match yaml::YamlLoader::load_from_str(&contents) {
            Ok(documents) => documents,
            Err(e) => {
                return Ok(Self::invalid(
                    connection, format!("YAML parse error in request: {}", e)));
            },
        };
        if documents.len() != 1 {
            return Ok(Self::invalid(
                connection,
                format!("illegal number of requests: {}", documents.len())));
        }
        let map = match documents.pop().expect("no documents") {
            yaml::Yaml::Hash(map) => map,
            _ => {
                return Ok(Self::invalid(connection,
                                        "request is not a map".into()));
            },
        };
        let req = Self::parse(connection, map);
        println!("# parsed into request");
        Ok(req)
    }

    fn parse(connection: unet::UnixStream, map: yaml::yaml::Hash) -> Box<Self> {
        let request = match map.get(&yaml::Yaml::String("request".into())) {
            Some(yaml::Yaml::String(s)) => s.clone(),
            Some(value) => {
                return Self::invalid(
                    connection, format!("illegal 'request' type: {:?}", value));
            },
            None => return Self::invalid(connection, "no 'request'".into()),
        };
        match request.as_str() {
            "get" => Box::from(Get { connection, map }),
            "list-drivers"  => Box::from(ListDrivers { connection }),
            "set" => Box::from(Set { connection, map }),
            "set-update-interval" => {
                Box::from(SetUpdateInterval { connection, map })
            },
            _ => {
                Self::invalid(connection,
                              format!("illegal 'request' value: {:?}", request))
            },
        }
    }

    fn invalid(connection: unet::UnixStream, error_msg: String) -> Box<Self> {
        Box::from(Invalid {
            connection,
            response: Response::Error(error_msg),
        })
    }

    fn device_dir(map: &yaml::yaml::Hash) -> Result<path::PathBuf, Response> {
        let driver_name = Self::key_file_name(map, "driver")?;
        let device_name = Self::key_file_name(map, "device")?;

        let drivers_dir = path::Path::new(Self::DRIVERS_DIR);
        let device_dir = drivers_dir.join(driver_name).join(device_name);

        if device_dir.exists() {
            Ok(device_dir)
        } else {
            Err(Response::Error(format!("device and/or driver does not exist: \
                                         {:?}", device_dir)))
        }
    }

    fn key_file_name(map: &yaml::yaml::Hash, key: &str) ->
        Result<ffi::OsString, Response>
    {
        let value = match map.get(&yaml::Yaml::String(key.into())) {
            Some(yaml::Yaml::String(s)) => s,
            Some(value) => {
                return Err(Response::Error(
                    format!("illegal '{}' type: {:?}", key, value)));
            },
            None => return Err(Response::Error(format!("no '{}'", key))),
        };
        match path::Path::new(value).file_name() {
            Some(file_name) => Ok(file_name.into()),
            None => {
                Err(Response::Error(format!("invalid '{}': {:?}", key, value)))
            },
        }
    }
}

struct Invalid {
    connection: unet::UnixStream,
    response: Response,
}

impl RequestInner for Invalid {
    fn connection(&mut self) -> &mut unet::UnixStream {
        &mut self.connection
    }

    fn execute(&self) -> Res<Response> {
        Ok(self.response.clone())
    }
}

struct Get {
    connection: unet::UnixStream,
    map: yaml::yaml::Hash,
}

impl Get {
    const ATTRIBUTES_FORBIDDEN: &'static [&'static str] = &["update_indicator"];
}

impl RequestInner for Get {
    fn connection(&mut self) -> &mut unet::UnixStream {
        &mut self.connection
    }

    fn execute(&self) -> Res<Response> {
        let device_dir = match RequestInner::device_dir(&self.map) {
            Ok(dir) => dir,
            Err(response) => return Ok(response),
        };

        let attribute_name = match RequestInner::key_file_name(&self.map,
                                                               "attribute") {
            Ok(file_name) => file_name,
            Err(response) => return Ok(response),
        };
        if Self::ATTRIBUTES_FORBIDDEN.iter()
            .find(|&&s| ffi::OsStr::new(s) == attribute_name)
            .is_some()
        {
            return Ok(Response::Error(
                format!("forbidden attribute: {:?}", attribute_name)));
        }

        let path = device_dir.join(attribute_name);
        let mut file = match fs::File::open(&path) {
            Ok(file) => file,
            Err(e) => return Ok(Response::Error(
                match e.kind() {
                    io::ErrorKind::NotFound => {
                        format!("attribute not found: {} ({:?})", e, path)
                    },
                    io::ErrorKind::PermissionDenied => {
                        format!("insufficient permissions to access attribute: \
                                 {} ({:?})", e, path)
                    },
                    _ => format!("cannot access attribute: {} ({:?})", e, path),
            })),
        };
        let value = {
            let mut buf = String::new();
            if let Err(e) = file.read_to_string(&mut buf) {
                return Ok(Response::Error(
                    format!("cannot read attribute: {} ({:?})", e, path)));
            }
            buf.trim_right().to_owned()
        };

        let response = {
            let mut response = yaml::yaml::Hash::new();
            response.insert(yaml::Yaml::String("value".into()),
                            yaml::Yaml::String(value));
            response
        };
        Ok(Response::Success(response))
    }
}

struct ListDrivers {
    connection: unet::UnixStream,
}

impl RequestInner for ListDrivers {
    fn connection(&mut self) -> &mut unet::UnixStream {
        &mut self.connection
    }

    fn execute(&self) -> Res<Response> {
        let drivers_dir = path::Path::new(RequestInner::DRIVERS_DIR);
        const DRIVER_NAMES: &[&'static str] = &["kraken", "kraken_x62"];

        let mut drivers = yaml::yaml::Hash::new();
        for &name in DRIVER_NAMES {
            let driver_name = path::Path::new(name);
            let driver_path = drivers_dir.join(driver_name);
            let entries = match fs::read_dir(driver_path) {
                Ok(entries) => entries,
                Err(_) => continue,
            };
            let mut devices = vec![];
            for entry in entries {
                let entry = match entry {
                    Ok(entry) => entry,
                    Err(_) => continue,
                };
                let device_name = match entry.file_name().into_string() {
                    Ok(name) => name,
                    Err(name) => {
                        return Ok(Response::Error(
                            format!("cannot convert device name to string: \
                                     {:?}", name)));
                    },
                };
                if !device_name.contains(":") {
                    continue;
                }
                devices.push(yaml::Yaml::String(device_name.into()));
            }
            drivers.insert(yaml::Yaml::String(name.into()),
                           yaml::Yaml::Array(devices));
        }

        let response = {
            let mut response = yaml::yaml::Hash::new();
            response.insert(yaml::Yaml::String("driver".into()),
                            yaml::Yaml::Hash(drivers));
            response
        };
        Ok(Response::Success(response))
    }
}

struct Set {
    connection: unet::UnixStream,
    #[allow(dead_code)]
    map: yaml::yaml::Hash,
}

impl RequestInner for Set {
    fn connection(&mut self) -> &mut unet::UnixStream {
        &mut self.connection
    }

    fn execute(&self) -> Res<Response> {
        Ok(Response::Error("TODO".into()))
    }
}

struct SetUpdateInterval {
    connection: unet::UnixStream,
    #[allow(dead_code)]
    map: yaml::yaml::Hash,
}

impl RequestInner for SetUpdateInterval {
    fn connection(&mut self) -> &mut unet::UnixStream {
        &mut self.connection
    }

    fn execute(&self) -> Res<Response> {
        Ok(Response::Error("TODO".into()))
    }
}

#[derive(Clone)]
enum Response {
    Success(yaml::yaml::Hash),
    Error(String),
}

impl Response {
    fn send(&self, connection: &mut unet::UnixStream) -> Res<()> {
        let response = match self {
            Response::Error(msg) => {
                let mut map = yaml::yaml::Hash::new();
                map.insert(yaml::Yaml::String("error".into()),
                           yaml::Yaml::String(msg.clone()));
                map
            },
            Response::Success(map) => {
                let mut map = map.clone();
                map.insert(yaml::Yaml::String("error".into()),
                           yaml::Yaml::Null);
                map
            },
        };

        let response = {
            let mut s = String::new();
            yaml::YamlEmitter::new(&mut s)
                .dump(&yaml::Yaml::Hash(response))?;
            s.push('\n');
            s.push_str(RequestInner::YAML_DOCUMENT_END_MARKER);
            s.push('\n');
            s
        };
        println!("# response: {}", response);

        connection.write_all(response.as_bytes())
            .map_err(|e| e.into())
    }
}
