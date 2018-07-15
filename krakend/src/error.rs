/// Program-specific errors.

use clap;
use libc;
use std::{error, ffi, fmt, io, path, string};
use xdg;

pub type Res<T> = Result<T, Error>;

#[derive(Debug)]
pub enum Error {
    Chmod(Errno, path::PathBuf),
    Chown(Errno, path::PathBuf),
    Clap(clap::Error),
    ClapDisplayed(clap::Error),
    Fork(Errno),
    Forked(libc::pid_t),
    Gid(Errno, ::Group),
    GidNotFound(::Group),
    Io(io::Error),
    ListenFds(Errno),
    ListenFdsKind,
    ListenFdsNr(usize),
    Notify(Errno),
    NotifyNoSocket,
    PidFileSize(path::PathBuf, usize),
    ProgramDirAcquire(path::PathBuf),
    Utf8(string::FromUtf8Error),
    Xdg(xdg::BaseDirectoriesError),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Chmod(e, path) => {
                write!(f, "cannot chmod {:?}: {}", path, e)
            }
            Error::Chown(e, path) => {
                write!(f, "cannot chown {:?}: {}", path, e)
            },
            Error::Clap(e) => e.fmt(f),
            Error::ClapDisplayed(e) => e.fmt(f),
            Error::Fork(e) => write!(f, "cannot fork: {}", e),
            Error::Forked(pid) => {
                write!(f, "forked process: child PID {}", pid)
            },
            Error::Gid(e, group) => {
                write!(f, "cannot access GID of group {}: {}", group, e)
            },
            Error::GidNotFound(group) => {
                write!(f, "GID of group {} not found", group)
            },
            Error::Io(e) => e.fmt(f),
            Error::ListenFds(e) => {
                write!(f, "cannot access listen sockets passed: {}", e)
            },
            Error::ListenFdsKind => {
                write!(f, "illegal kind of listen socket passed")
            },
            Error::ListenFdsNr(nr) => {
                write!(f, "illegal nr of listen sockets passed: {}", nr)
            },
            Error::Notify(e) => write!(f, "cannot notify: {}", e),
            Error::NotifyNoSocket => write!(f, "cannot notify: socket not set"),
            Error::PidFileSize(path, size) => {
                write!(f, "PID file {:?} too large: {} B", path, size)
            },
            Error::ProgramDirAcquire(path) => {
                write!(f, "cannot acquire program dir {:?}", path)
            },
            Error::Utf8(e) => e.fmt(f),
            Error::Xdg(e) => e.fmt(f),
        }
    }
}

impl error::Error for Error {
    fn cause(&self) -> Option<&error::Error> {
        match self {
            Error::Clap(e) |
            Error::ClapDisplayed(e) => Some(e),
            Error::Io(e) => Some(e),
            Error::Utf8(e) => Some(e),
            Error::Xdg(e) => Some(e),
            Error::Chmod(..) |
            Error::Chown(..) |
            Error::Fork(..) |
            Error::Forked(..) |
            Error::Gid(..) |
            Error::GidNotFound(..) |
            Error::ListenFds(..) |
            Error::ListenFdsKind |
            Error::ListenFdsNr(..) |
            Error::Notify(..) |
            Error::NotifyNoSocket |
            Error::PidFileSize(..) |
            Error::ProgramDirAcquire(..) => None,
        }
    }
}

impl From<clap::Error> for Error {
    fn from(e: clap::Error) -> Self {
        match e.kind {
            clap::ErrorKind::HelpDisplayed |
            clap::ErrorKind::VersionDisplayed => Error::ClapDisplayed(e),
            _ => Error::Clap(e),
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error::Io(e)
    }
}

impl From<string::FromUtf8Error> for Error {
    fn from(e: string::FromUtf8Error) -> Self {
        Error::Utf8(e)
    }
}

impl From<xdg::BaseDirectoriesError> for Error {
    fn from(e: xdg::BaseDirectoriesError) -> Self {
        Error::Xdg(e)
    }
}

#[derive(Debug)]
pub struct Errno {
    errno: libc::c_int,
}

impl Errno {
    pub fn from_global_errno() -> Self {
        // NOTE: unsafe is OK, as errno is always safe to access (though
        // possibly not thread-safe)
        let errno = unsafe {
            *libc::__errno_location().as_ref()
                .expect("errno location is NULL")
        };
        Self::from(errno)
    }
}

impl fmt::Display for Errno {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // NOTE: unsafe is ok, as strerror() returns valid strings for all
        // values (though not thread-safe)
        let msg = unsafe {
            ffi::CStr::from_ptr(libc::strerror(self.errno))
        }.to_str()
            .map_err(|_| fmt::Error::default())?;
        write!(f, "{} (errno {})", msg, self.errno)
    }
}

impl From<libc::c_int> for Errno {
    fn from(errno: libc::c_int) -> Self {
        Self { errno }
    }
}
