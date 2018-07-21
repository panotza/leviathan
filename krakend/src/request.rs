use error::Res;
use std::{ffi, fmt, fs, io, path, time};
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
        const TIMEOUT_MS: u64 = 250;
        if let Err(e) = connection.set_read_timeout(
            Some(time::Duration::from_millis(TIMEOUT_MS))
        ) {
            return Some(Err(e.into()));
        };
        let inner = {
            let input = io::BufReader::new(&connection);
            match RequestInner::receive(input) {
                Ok(inner) => inner,
                Err(e) => return Some(Err(e)),
            }
        };
        Some(Ok(Request { connection, inner }))
    }
}

pub struct Request {
    connection: unet::UnixStream,
    inner: Box<RequestInner>,
}

impl Request {
    pub fn execute(self) -> Res<()> {
        let response = self.inner.execute()?;
        println!("# response: {}", response);
        write!(io::BufWriter::new(self.connection), "{}", response)?;
        Ok(())
    }
}

trait RequestInner {
    fn execute(&self) -> Res<Response>;
}

impl RequestInner {
    const YAML_DOCUMENT_END_MARKER: &'static str = "...";

    fn receive<R>(input: R) -> Res<Box<Self>>
    where R: io::BufRead
    {
        println!("# receiving into request ...");
        let contents = {
            let mut contents = String::new();
            for line in input.lines() {
                let line = line?;
                if line.starts_with(Self::YAML_DOCUMENT_END_MARKER) {
                    break;
                } else {
                    contents.push_str(&line);
                }
            }
            contents
        };
        println!("# received contents");
        let mut documents = match yaml::YamlLoader::load_from_str(&contents) {
            Ok(documents) => documents,
            Err(e) => {
                return Ok(Self::invalid(
                    format!("YAML parse error in request: {}", e)));
            },
        };
        println!("# parsed YAML");
        let map = match documents.pop() {
            Some(yaml::Yaml::Hash(map)) => map,
            Some(_) => {
                return Ok(Self::invalid("request is not a map".into()));
            },
            None => return Ok(Self::invalid("no request".into())),
        };
        if documents.pop().is_some() {
            return Ok(Self::invalid("multiple requests".into()));
        }
        let req = Self::parse(map);
        println!("# parsed into request");
        Ok(req)
    }

    fn parse(map: yaml::yaml::Hash) -> Box<Self> {
        let request = match map.get(&yaml::Yaml::String("request".into())) {
            Some(yaml::Yaml::String(s)) => s.clone(),
            Some(value) => {
                return Self::invalid(
                    format!("illegal type for 'request': {:?}", value));
            },
            None => return Self::invalid("'request' not present".into()),
        };
        match request.as_str() {
            "get" => Box::from(Get { map }),
            "list-drivers"  => Box::from(ListDrivers { }),
            "set" => Box::from(Set { map }),
            "set-update-interval" => {
                Box::from(SetUpdateInterval { map })
            },
            _ => {
                Self::invalid(
                    format!("illegal value for 'request': {:?}", request))
            },
        }
    }

    fn invalid(error_msg: String) -> Box<Self> {
        Box::from(Invalid {
            response: Response::Error(error_msg),
        })
    }

    const DRIVER_NAMES: [&'static str; 2] = ["kraken", "kraken_x62"];
    const DRIVERS_DIR: &'static str = "/sys/bus/usb/drivers";
    const ATTRIBUTES_DIR: &'static str = "kraken";

    fn get_attribute_path<'a, I>(map: &yaml::yaml::Hash, forbidden: I) ->
        Result<path::PathBuf, Response>
    where I: IntoIterator<Item = &'a str>
    {
        let attributes_dir = Self::get_attributes_dir(map)?;
        let attribute_name = Self::get_file_name(map, "attribute")?;
        let attribute_name = Self::as_attribute_name(&attribute_name,
                                                     forbidden)?
            .ok_or_else(|| Response::Error(
                format!("forbidden attribute name: {:?}", attribute_name)))?;
        Ok(attributes_dir.join(attribute_name))
    }

    fn get_attributes_dir(map: &yaml::yaml::Hash) ->
        Result<path::PathBuf, Response>
    {
        let driver_name = Self::get_file_name(map, "driver")?;
        let driver_name = Self::as_driver_name(&driver_name)?
            .ok_or_else(|| Response::Error(
                format!("illegal driver name: {:?}", driver_name)))?;

        let device_name = Self::get_file_name(map, "device")?;
        let device_name = Self::as_device_name(&device_name)?
            .ok_or_else(|| Response::Error(
                format!("illegal device name: {:?}", device_name)))?;

        let drivers_dir = path::Path::new(Self::DRIVERS_DIR);
        let attributes_dir = path::Path::new(Self::ATTRIBUTES_DIR);

        let attributes_dir: path::PathBuf = [
            drivers_dir, driver_name.as_ref(), device_name.as_ref(),
            attributes_dir
        ].iter().collect();

        if attributes_dir.exists() {
            Ok(attributes_dir)
        } else {
            Err(Response::Error(format!("driver and/or device does not exist: \
                                         {:?}", attributes_dir)))
        }
    }

    fn get_file_name(map: &yaml::yaml::Hash, key: &str) ->
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
            Some(file_name) => Ok(file_name.to_owned().into()),
            None => {
                Err(Response::Error(format!("invalid '{}': {:?}", key, value)))
            },
        }
    }

    fn as_driver_name(file_name: &ffi::OsStr) ->
        Result<Option<&ffi::OsStr>, Response>
    {
        Ok(Self::DRIVER_NAMES.iter()
           .find(|s| ffi::OsStr::new(s) == file_name)
           .and(Some(file_name)))
    }

    fn as_device_name(file_name: &ffi::OsStr) ->
        Result<Option<String>, Response>
    {
        let file_name = file_name.to_str()
            .ok_or_else(|| Response::Error(
                format!("cannot convert device name to string: {:?}",
                        file_name)))?
            .to_owned();
        if file_name.contains(":") {
            Ok(Some(file_name))
        } else {
            Ok(None)
        }
    }

    fn as_attribute_name<'a, I>(file_name: &ffi::OsStr, forbidden: I) ->
        Result<Option<&ffi::OsStr>, Response>
    where I: IntoIterator<Item = &'a str>
    {
        Ok(match forbidden.into_iter()
           .find(|s| ffi::OsStr::new(s) == file_name) {
               Some(_) => None,
               None => Some(file_name)
           })
    }
}

struct Invalid {
    response: Response,
}

impl RequestInner for Invalid {
    fn execute(&self) -> Res<Response> {
        Ok(self.response.clone())
    }
}

struct Get {
    map: yaml::yaml::Hash,
}

impl Get {
    const ATTRIBUTES_FORBIDDEN: [&'static str; 1] = ["update_sync"];
}

impl RequestInner for Get {
    fn execute(&self) -> Res<Response> {
        let path = match RequestInner::get_attribute_path(
            &self.map, Self::ATTRIBUTES_FORBIDDEN.iter().map(|&s| s)
        ) {
            Ok(path) => path,
            Err(response) => return Ok(response),
        };

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

struct ListDrivers { }

impl RequestInner for ListDrivers {
    fn execute(&self) -> Res<Response> {
        let drivers_dir = path::Path::new(RequestInner::DRIVERS_DIR);
        let mut drivers = yaml::yaml::Hash::new();

        for &name in RequestInner::DRIVER_NAMES.iter() {
            let driver_name = path::Path::new(name);
            let driver_path = drivers_dir.join(driver_name);
            let entries = match fs::read_dir(driver_path) {
                Ok(entries) => entries,
                Err(_) => continue,
            };
            let mut devices = vec![];
            for entry in entries {
                let device_name = match entry {
                    Ok(entry) => entry.file_name(),
                    Err(_) => continue,
                };
                let device_name =
                    match RequestInner::as_device_name(&device_name) {
                        Ok(Some(device_name)) => device_name,
                        Ok(None) => continue,
                        Err(response) => return Ok(response),
                    };
                devices.push(yaml::Yaml::String(device_name.into()));
            }
            drivers.insert(yaml::Yaml::String(name.into()),
                           yaml::Yaml::Array(devices));
        }

        let response = {
            let mut response = yaml::yaml::Hash::new();
            response.insert(yaml::Yaml::String("drivers".into()),
                            yaml::Yaml::Hash(drivers));
            response
        };
        Ok(Response::Success(response))
    }
}

struct Set {
    #[allow(dead_code)]
    map: yaml::yaml::Hash,
}

impl RequestInner for Set {
    fn execute(&self) -> Res<Response> {
        Ok(Response::Error("TODO".into()))
    }
}

struct SetUpdateInterval {
    #[allow(dead_code)]
    map: yaml::yaml::Hash,
}

impl RequestInner for SetUpdateInterval {
    fn execute(&self) -> Res<Response> {
        Ok(Response::Error("TODO".into()))
    }
}

#[derive(Clone)]
enum Response {
    Success(yaml::yaml::Hash),
    Error(String),
}

impl fmt::Display for Response {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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

        yaml::YamlEmitter::new(f)
            .dump(&yaml::Yaml::Hash(response))
            .map_err(|_| fmt::Error {})?;
        write!(f, "\n{}\n", RequestInner::YAML_DOCUMENT_END_MARKER)
    }
}
