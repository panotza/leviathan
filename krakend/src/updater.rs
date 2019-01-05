/// Updater which controls driver devices, and the JSON-RPC methods its uses.

use jsonrpc_core::futures::future::{self, IntoFuture};
use jsonrpc_core as jrpc;
use std::ops::Deref;
use std::{fs, path, sync};

use JRPC_COMPATIBILITY;

type JrpcFutureResult = future::FutureResult<jrpc::Value, jrpc::Error>;

const DRIVERS_SUPPORTED: [&'static str; 2] = ["kraken", "kraken_x62"];
const DRIVERS_DIR: &'static str = "/sys/bus/usb/drivers";
const ATTRIBUTES_DIR: &'static str = "kraken";

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
    device: sync::RwLock<Option<Device>>,
}

impl Updater {
    fn new() -> Self {
        let rpc_handler =
            jrpc::IoHandler::with_compatibility(JRPC_COMPATIBILITY);
        Self {
            rpc_handler: rpc_handler.into(),
            methods_added: false.into(),
            device: None.into(),
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
        rpc_handler.add_method("managedDevice",
                               move |p| self.managed_device(p));
        rpc_handler.add_method("setManagedDevice",
                               move |p| self.set_managed_device(p));

        // NOTE: [unwrap] Forward panic.
        let mut methods_added = self.methods_added.write().unwrap();
        *methods_added = true;
    }

    fn available_devices(&self, params: jrpc::Params) -> JrpcFutureResult {
        if let jrpc::Params::None = params {
        } else {
            return Error::ParamsNotNone.into_future();
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
                let mut device_map = serde_json::Map::new();
                device_map.insert("driver".into(), driver.into());
                device_map.insert("device".into(), file_name.into());
                devices.push(device_map);
            }
        }

        Ok(devices)
    }

    fn managed_device(&self, params: jrpc::Params) -> JrpcFutureResult {
        if let jrpc::Params::None = params {
        } else {
            return Error::ParamsNotNone.into_future();
        }
        self.managed_device_impl()
            .map(|v| match v {
                Some(map) => map.into(),
                None => jrpc::Value::Null,
            })
            .into()
    }

    fn managed_device_impl(
        &self
    ) -> jrpc::Result<Option<serde_json::Map<String, jrpc::Value>>>
    {
        // NOTE: [unwrap] Forward panic.
        let device = self.device.read().unwrap();
        match device.deref() {
            Some(device) => {
                let mut map = serde_json::Map::new();
                map.insert("driver".into(), device.driver.clone().into());
                map.insert("device".into(), device.device.clone().into());
                Ok(Some(map))
            },
            None => Ok(None),
        }
    }

    fn set_managed_device(&self, params: jrpc::Params) -> JrpcFutureResult {
        let params = if let jrpc::Params::Map(map) = params {
            map
        } else {
            return Error::ParamsNotObject.into_future();
        };

        let mut device = None;
        let mut driver = None;
        for (key, value) in params.into_iter() {
            match key.as_str() {
                "device" => match value {
                    jrpc::Value::String(s) => device = Some(s),
                    _ => {
                        return Error::params_key_type(key.clone(), "string")
                            .into_future();
                    },
                },
                "driver" => match value {
                    jrpc::Value::String(s) => driver = Some(s),
                    _ => {
                        return Error::params_key_type(key.clone(), "string")
                            .into_future();
                    },
                },
                _ => {
                    return Error::ParamsKeyUnknown(key.clone()).into_future();
                },
            };
        }
        let device_name = match device {
            Some(device) => device,
            None => {
                return Error::ParamsKeyNotPresent("device").into_future();
            },
        };
        let driver_name = match driver {
            Some(driver) => driver,
            None => {
                return Error::ParamsKeyNotPresent("driver").into_future();
            },
        };

        self.set_managed_device_impl(&driver_name, &device_name)
            .map(|_| jrpc::Value::Null)
            .into()
    }

    fn set_managed_device_impl(&self, driver_name: &str, device_name: &str)
                               -> jrpc::Result<()> {
        if !DRIVERS_SUPPORTED.contains(&driver_name) {
            return Err(Error::DriverNotSupported.into());
        }
        let device_filename = match path::Path::new(device_name).file_name() {
            Some(s) => match s.to_str() {
                Some(s) => s,
                None => return Err(Error::DeviceNameInvalid.into()),
            },
            None => return Err(Error::DeviceNameInvalid.into()),
        };
        if device_name != device_filename {
            return Err(Error::DeviceNameInvalid.into());
        }
        let device_name = device_filename;

        // NOTE: [unwrap] Forward panic.
        let mut device = self.device.write().unwrap();
        *device = Some(Device {
            driver: driver_name.into(),
            device: device_name.into()
        });
        println!("info: managing  driver {:?}, device {:?}",
                 driver_name, device_name);
        Ok(())
    }
}

/// A device of a driver.
struct Device {
    driver: String,
    device: String,
}

impl Device {
    fn attributes_dir(&self) -> path::PathBuf {
        let mut dir = path::PathBuf::from(DRIVERS_DIR);
        dir.push(&self.driver);
        dir.push(&self.device);
        dir.push(ATTRIBUTES_DIR);
        dir
    }
}

/// Errors specific to this module.
enum Error {
    DeviceNameInvalid,
    DriverNotSupported,
    ParamsKeyNotPresent(&'static str),
    ParamsKeyType { key: String, type_: &'static str },
    ParamsKeyUnknown(String),
    ParamsNotNone,
    ParamsNotObject,
}

impl Error {
    fn params_key_type(key: String, type_: &'static str) -> Self {
        Error::ParamsKeyType { key, type_ }
    }
}

impl From<Error> for jrpc::Error {
    fn from(error: Error) -> Self {
        match error {
            Error::DeviceNameInvalid => jrpc::Error::invalid_params(
                "Device name is invalid",
            ),
            Error::DriverNotSupported => jrpc::Error::invalid_params(
                "Driver not supported",
            ),
            Error::ParamsKeyNotPresent(key) => {
                jrpc::Error::invalid_params(format!(
                    "Expected key {:?} not present", key,
                ))
            },
            Error::ParamsKeyType { key, type_ } => {
                jrpc::Error::invalid_params(format!(
                    "Value of key {:?} is not of type {}", key, type_,
                ))
            },
            Error::ParamsKeyUnknown(key) => {
                jrpc::Error::invalid_params(format!(
                    "Unexpected key {:?} present", key,
                ))
            },
            Error::ParamsNotNone => jrpc::Error::invalid_params(
                "Unexpected params present",
            ),
            Error::ParamsNotObject => jrpc::Error::invalid_params(
                "Params is not of type object",
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
