/// Updater which controls driver devices, and the JSON-RPC methods its uses.

use jsonrpc_core::futures::future::{self, IntoFuture};
use jsonrpc_core as jrpc;
use serde_json;
use std::{fs, path, sync};

use JRPC_COMPATIBILITY;

type JrpcFutureResult = future::FutureResult<jrpc::Value, jrpc::Error>;

const DRIVERS_SUPPORTED: [&'static str; 2] = ["kraken", "kraken_x62"];
const DRIVERS_DIR: &'static str = "/sys/bus/usb/drivers";

lazy_static!{
    static ref UPDATER: Updater = Updater::new();
}

pub fn rpc_handler() -> &'static sync::RwLock<jrpc::IoHandler> {
    UPDATER.add_methods();
    &UPDATER.rpc_handler
}

struct Updater {
    rpc_handler: sync::RwLock<jrpc::IoHandler>,
    methods_added: sync::RwLock<bool>,
}

impl Updater {
    fn new() -> Self {
        let rpc_handler =
            jrpc::IoHandler::with_compatibility(JRPC_COMPATIBILITY);
        Self {
            rpc_handler: rpc_handler.into(),
            methods_added: false.into(),
        }
    }

    fn add_methods(&'static self) {
        {
            // NOTE: [unwrap] Forward panic.
            let methods_added = self.methods_added.read().unwrap();
            if *methods_added {
                return;
            }
        }

        // NOTE: [unwrap] Forward panic.
        let mut rpc_handler = self.rpc_handler.write().unwrap();
        rpc_handler.add_method("availableDevices",
                               move |p| self.available_devices(p));
        rpc_handler.add_method("getUpdateInterval",
                               move |p| self.get_update_interval(p));

        // NOTE: [unwrap] Forward panic.
        let mut methods_added = self.methods_added.write().unwrap();
        *methods_added = true;
    }

    fn available_devices(&self, params: jrpc::Params) -> JrpcFutureResult {
        match params {
            jrpc::Params::None => (),
            _ => return Error::ParamsType.into_future(),
        };
        self.available_devices_impl().map(|v| v.into()).into()
    }

    fn available_devices_impl(
        &self
    ) -> jrpc::Result<Vec<serde_json::Map<String, jrpc::Value>>>
    {
        let mut devices = vec![];

        let drivers_dir = path::Path::new(DRIVERS_DIR);
        for &driver in DRIVERS_SUPPORTED.iter() {
            let dir = drivers_dir.join(driver);
            let entries = match fs::read_dir(&dir) {
                Ok(entries) => entries,
                Err(e) => {
                    eprintln!("info: driver dir {:?} inaccessible: {}", dir, e);
                    continue;
                },
            };

            for entry in entries {
                let entry = match entry {
                    Ok(entry) => entry,
                    Err(e) => {
                        eprintln!("warn: driver dir entry inaccessible: {}", e);
                        continue;
                    },
                };
                let path = entry.path();
                // NOTE: [expect] This should never happen
                let file_name = path.file_name()
                    .expect("dir entry file name is None");
                let file_name = match file_name.to_str() {
                    Some(name) => name,
                    None => {
                        eprintln!(
                            "warn: driver dir entry not valid unicode: {:?}",
                            file_name,
                        );
                        continue;
                    },
                };
                if !file_name.contains(':') {
                    continue;
                }
                let metadata = match path.metadata() {
                    Ok(metadata) => metadata,
                    Err(e) => {
                        eprintln!("warn: device file {:?} inaccessible: {}",
                                  path, e);
                        continue;
                    }
                };
                if !metadata.is_dir() {
                    eprintln!("warn: device file {:?} is not directory", path);
                    continue;
                }
                devices.push(Device::new(driver, file_name).into());
            }
        }

        Ok(devices)
    }

    fn get_update_interval(&self, params: jrpc::Params) -> JrpcFutureResult {
        let params = match params {
            jrpc::Params::Map(map) => map,
            _ => return Error::ParamsType.into_future(),
        };
        let mut device = None;
        for (key, value) in params.into_iter() {
            match key.as_str() {
                "device" => device = Some(value),
                _ => return Error::params_key_unexpected(key).into_future(),
            };
        }
        let device = match self.params_device(device) {
            Ok(device) => device,
            Err(e) => return e.into_future(),
        };
        self.get_update_interval_impl(device).map(|v| v.into()).into()
    }

    fn get_update_interval_impl(&self, device: Device) -> jrpc::Result<u64> {
        Ok(device.get_attribute_unrestricted("update_interval")?)
    }

    fn params_device(&self, device_map: Option<jrpc::Value>)
                     -> Result<Device, Error> {
        let device_map = match device_map {
            Some(value) => value,
            None => return Err(Error::params_key_type("device")),
        };
        Device::try_from(device_map)
    }
}

/// A device of a driver.
struct Device {
    driver: String,
    device: String,
}

impl Device {
    fn new<S, T>(driver: S, device: T) -> Self
    where S: Into<String>, T: Into<String>,
    {
        Self {
            driver: driver.into(),
            device: device.into(),
        }
    }

    fn get_attribute_unrestricted<T>(&self, name: &str) -> Result<T, Error>
    where T: std::str::FromStr,
    <T as std::str::FromStr>::Err: ToString,
    {
        let path = {
            let mut path = self.attributes_dir();
            path.push(name);
            path
        };
        let contents = match fs::read(path) {
            Ok(contents) => contents,
            Err(e) => return Err(Error::device_not_accessible(e.to_string())),
        };
        String::from_utf8(contents)
            .map_err(|e| Error::attribute_read(name, e.to_string()))?
            .trim()
            .parse::<T>()
            .map_err(|e| Error::attribute_read(name, e.to_string()))
    }

    fn attributes_dir(&self) -> path::PathBuf {
        const ATTRIBUTES_DIR: &'static str = "kraken";

        let mut dir = path::PathBuf::from(DRIVERS_DIR);
        dir.push(&self.driver);
        dir.push(&self.device);
        dir.push(ATTRIBUTES_DIR);
        dir
    }
}

// LATER: Switch to TryFrom<jrpc::Value> when stable.
impl Device {
    fn try_from(value: jrpc::Value) -> Result<Self, Error> {
        let device_map = match value {
            jrpc::Value::Object(map) => map,
            _ => return Err(Error::ParamsDeviceFormat),
        };

        let mut device = None;
        let mut driver = None;
        for (key, value) in device_map.into_iter() {
            match key.as_str() {
                "device" => match value {
                    jrpc::Value::String(s) => device = Some(s),
                    _ => return Err(Error::ParamsDeviceFormat),
                },
                "driver" => match value {
                    jrpc::Value::String(s) => driver = Some(s),
                    _ => return Err(Error::ParamsDeviceFormat),
                },
                _ => return Err(Error::ParamsDeviceFormat),
            };
        }
        let device = match device {
            Some(device) => device,
            None => return Err(Error::ParamsDeviceFormat),
        };
        let driver = match driver {
            Some(driver) => driver,
            None => return Err(Error::ParamsDeviceFormat),
        };

        if !DRIVERS_SUPPORTED.contains(&driver.as_str()) {
            return Err(Error::DriverNotSupported);
        }
        let device_filename = match path::Path::new(&device).file_name() {
            Some(s) => match s.to_str() {
                Some(s) => s,
                None => return Err(Error::ParamsDeviceName),
            },
            None => return Err(Error::ParamsDeviceName),
        };
        if device != device_filename {
            return Err(Error::ParamsDeviceName);
        }
        let device = device_filename.to_string();

        let driver = driver.clone();
        Ok(Self { driver, device })
    }
}

impl From<Device> for serde_json::Map<String, jrpc::Value> {
    fn from(device: Device) -> Self {
        let mut map = Self::new();
        map.insert("driver".into(), device.driver.into());
        map.insert("device".into(), device.device.into());
        map
    }
}

/// Errors specific to this module.
enum Error {
    AttributeRead { attribute: String, data: String },
    DeviceNotAccessible { data: String },
    DriverNotSupported,
    ParamsDeviceFormat,
    ParamsDeviceName,
    ParamsKeyType { key: String },
    ParamsKeyUnexpected { key: String },
    ParamsType,
}

impl Error {
    fn attribute_read<S, T>(attribute: S, data: T) -> Self
    where S: Into<String>, T: Into<String>,
    {
        Error::AttributeRead {
            attribute: attribute.into(),
            data: data.into(),
        }
    }

    fn device_not_accessible<S>(data: S) -> Self
    where S: Into<String>,
    {
        Error::DeviceNotAccessible { data: data.into() }
    }

    fn params_key_type<S>(key: S) -> Self
    where S: Into<String>,
    {
        Error::ParamsKeyType { key: key.into() }
    }

    fn params_key_unexpected<S>(key: S) -> Self
    where S: Into<String>,
    {
        Error::ParamsKeyUnexpected { key: key.into() }
    }
}

impl From<Error> for jrpc::Error {
    fn from(error: Error) -> Self {
        match error {
            Error::AttributeRead { attribute, data } => jrpc::Error {
                code: 1.into(),
                data: Some(data.into()),
                message: format!("Failed to read attribute {:?}", attribute),
            },
            Error::DeviceNotAccessible { data } => jrpc::Error {
                code: 2.into(),
                data: Some(data.into()),
                message: "Device not accessible".into(),
            },
            Error::DriverNotSupported => jrpc::Error {
                code: 0.into(),
                data: None,
                message: "Driver not supported".into(),
            },
            Error::ParamsDeviceFormat => jrpc::Error::invalid_params(
                "Device format is invalid",
            ),
            Error::ParamsDeviceName => jrpc::Error::invalid_params(
                "Device name is invalid",
            ),
            Error::ParamsKeyType { key } => {
                jrpc::Error::invalid_params(format!(
                    "Key {:?} is not of expected type", key,
                ))
            },
            Error::ParamsKeyUnexpected { key } => {
                jrpc::Error::invalid_params(format!(
                    "Key {:?} not expected", key,
                ))
            },
            Error::ParamsType => jrpc::Error::invalid_params(
                "Params is not of expected type",
            ),
        }
    }
}

impl IntoFuture for Error {
    type Future = JrpcFutureResult;

    type Error = <Self::Future as future::Future>::Error;
    type Item = <Self::Future as future::Future>::Item;

    fn into_future(self) -> Self::Future {
        Err(self.into()).into()
    }
}
