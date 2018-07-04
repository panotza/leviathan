use c;
use clap;
use error::{Error, Res};
use std::os::unix::io::FromRawFd;
use std::os::unix::net as unet;

pub struct InitSystem {
    daemonize: bool,
    notify: Notify,
}

impl InitSystem {
    pub fn new(daemonize: Option<bool>) -> Res<Self> {
        let daemonize = match daemonize {
            Some(daemonize) if daemonize => {
                return Err(clap::Error::with_description(
                    "cannot daemonize with systemd",
                    clap::ErrorKind::ArgumentConflict,
                ).into());
            },
            Some(daemonize) => daemonize,
            None => false,
        };
        Ok(Self {
            daemonize,
            notify: Notify { },
        })
    }
}

impl super::InitSystem for InitSystem {
    fn daemonize(&self) -> bool {
        self.daemonize
    }

    fn notify(&self) -> &super::Notify {
        &self.notify
    }

    fn socket_file(&self, _program_dir: &::ProgramDir) -> Res<::SocketFile> {
        let mut fds = c::systemd::listen_fds(false)
            .map_err(|e| Error::ListenFds(e))?;
        if fds.len() != 1 {
            return Err(Error::ListenFdsNr(fds.len() as usize));
        }
        let fd = fds.next()
            .expect("no file descriptor in fds of length 1");

        if !c::systemd::is_socket(fd, c::systemd::AddressFamily::Unix,
                                  c::systemd::SocketType::Stream,
                                  c::systemd::Listening::Listening)
            .map_err(|e| Error::ListenFds(e))?
        {
            return Err(Error::ListenFdsKind);
        }

        // NOTE: unsafe is OK, as fd is checked to be a listening, UNIX, STREAM
        // socket beforehand
        let listener = unsafe { unet::UnixListener::from_raw_fd(fd) };
        Ok(::SocketFile::new(listener, None))
    }
}

struct Notify { }

impl Notify {
    fn notify(&self, state: c::systemd::NotifyState) -> Res<()> {
        if !c::systemd::notify(false, state)
            .map_err(|e| Error::Notify(e))? {
            Err(Error::NotifyNoSocket)
        } else {
            Ok(())
        }
    }
}

impl super::Notify for Notify {
    fn startup_end(&self) -> Res<()> {
        self.notify([("READY", "1")].as_ref().into())
    }

    fn reload_start(&self) -> Res<()> {
        self.notify([("RELOADING", "1")].as_ref().into())
    }

    fn reload_end(&self) -> Res<()> {
        self.notify([("READY", "1")].as_ref().into())
    }

    fn shutdown_start(&self) -> Res<()> {
        self.notify([("STOPPING", "1")].as_ref().into())
    }
}
