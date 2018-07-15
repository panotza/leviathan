use c;
use error::{Error, Res};
use std::{fs, io, path};
use std::str::FromStr;
use std::os::unix::net as unet;

pub struct InitSystem {
    daemonize: bool,
    notify: Notify,
    socket_file_group: ::Group,
}

impl InitSystem {
    pub fn new(daemonize: Option<bool>, socket_file_group: ::Group) -> Self
    {
        Self {
            daemonize: daemonize.unwrap_or(true),
            notify: Notify { },
            socket_file_group,
        }
    }
}

impl super::InitSystem for InitSystem {
    fn daemonize(&self) -> bool {
        self.daemonize
    }

    fn notify(&self) -> &super::Notify {
        &self.notify
    }

    fn socket_file(&self, program_dir: &::ProgramDir) -> Res<::SocketFile> {
        const SOCKET_FILE_NAME: &str = "socket";
        let path = program_dir.join(path::Path::new(SOCKET_FILE_NAME));
        let listener = loop {
            match unet::UnixListener::bind(&path) {
                Ok(listener) => break listener,
                Err(e) => match e.kind() {
                    io::ErrorKind::AddrInUse => {
                        println!("info: socket {:?} already exists: removing",
                                 path);
                        fs::remove_file(&path)?;
                        continue;
                    },
                    _ => return Err(e.into()),
                },
            }
        };
        let path = path.canonicalize()?;
        // NOTE: `socket_file` must be created before any possible returns after
        // `canonicalize()` (which fails if the socket file does not exist, so
        // it's ok), so the socket file is automatically removed upon an error
        // return
        let socket_file = ::SocketFile::new(listener, Some(&path));

        let gid = self.socket_file_group.getgr()?.gid();
        c::linux::chown(&path, c::linux::ChownUid::Unspecified,
                        c::linux::ChownGid::Gid(gid))
            .map_err(|e| Error::Chown(e, path.clone()))?;
        const MODE: &'static str = "---rw-rw----";
        let mode = c::linux::ChmodMode::from_str(MODE)
            .expect("invalid chmod mode");
        c::linux::chmod(&path, mode)
            .map_err(|e| Error::Chmod(e, path.clone()))?;
        Ok(socket_file)
    }
}

struct Notify { }

impl super::Notify for Notify {
    fn startup_end(&self) -> Res<()> {
        Ok(())
    }

    fn reload_start(&self) -> Res<()> {
        Ok(())
    }

    fn reload_end(&self) -> Res<()> {
        Ok(())
    }

    fn shutdown_start(&self) -> Res<()> {
        Ok(())
    }
}
