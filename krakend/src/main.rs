#[macro_use]
extern crate clap;
extern crate libc;
extern crate xdg;

mod c;
mod error;
mod init_system;

use error::{Error, Res};
use init_system as is;
use std::{ffi, fmt, fs, io, path, process};
use std::io::prelude::*;
use std::os::unix::net as unet;
use std::str::FromStr;

fn main() {
    process::exit(match run() {
        Ok(_) => 0,
        Err(err) => match err {
            Error::Clap(e) => {
                print!("{}", e);
                2
            }
            Error::ClapDisplayed(e) => {
                print!("{}", e);
                0
            },
            Error::Forked(..) => {
                println!("{}: exiting parent", err);
                0
            },
            e => {
                eprintln!("{}: {}", crate_name!(), e);
                1
            },
        }
    })
}

fn run() -> Res<()> {
    let init_system_default = is::InitSystemKind::variant_default().to_string();
    let socket_file_group_default = Group::current().to_string();
    let args = clap::App::new(crate_name!())
        .about(crate_description!())
        .version(crate_version!())
        .arg(clap::Arg::with_name("daemonize")
             .long("daemonize")
             .help("Whether to spawn a child daemon process and close the \
                    parent; determined by the init-system if unspecified")
             .takes_value(true)
             .value_name("BOOLEAN"))
        .arg(clap::Arg::with_name("init-system")
             .long("init-system")
             .help("What init system to get socket from and otherwise confirm \
                    to (signals, status updates, etc.); None creates the \
                    socket file")
             .case_insensitive(true)
             .default_value(&init_system_default)
             .possible_values(&is::InitSystemKind::variants())
             .takes_value(true)
             .value_name("INIT-SYSTEM"))
        .arg(clap::Arg::with_name("socket-file-group")
             .long("socket-file-group")
             .help("The system group of the socket file, if created; either a \
                    name or a numeric id")
             .default_value(&socket_file_group_default)
             .takes_value(true)
             .value_name("GROUP"))
        .get_matches_safe()?;

    let init_system_kind = value_t!(args.value_of("init-system"),
                                    is::InitSystemKind)?;
    let init_system = init_system_kind.to_init_system(
        || match args.value_of("daemonize") {
            None => Ok(None),
            Some(s) => s.parse()
                .map(|b| Some(b))
                .map_err(|_| {
                    clap::Error::with_description(
                        "daemonize", clap::ErrorKind::InvalidValue).into()
                }),
        },
        || {
            value_t!(args.value_of("socket-file-group"), Group)
                .map_err(|e| e.into())
        },
    )?;

    if init_system.daemonize() {
        fork_and_exit()?;
        println!("forked process: in child: PID {}", process::id());
    }

    let program_dir = ProgramDir::acquire()?;

    let socket_file = init_system.socket_file(&program_dir)?;
    println!("socket_file: {:?}", socket_file);

    println!("startup complete");
    init_system.notify().startup_end()?;

    println!("# TODO: == main loop start ==");
    std::thread::sleep(std::time::Duration::from_secs(20));
    println!("# TODO: == main loop end ==");

    println!("shutdown starting");
    init_system.notify().shutdown_start()?;

    Ok(())
}

fn fork_and_exit() -> Res<()> {
    match c::linux::fork().map_err(|e| Error::Fork(e))? {
        c::linux::ForkPid::Parent => Ok(()),
        c::linux::ForkPid::Child(pid) => Err(Error::Forked(pid)),
    }
}

#[derive(Clone, Debug)]
pub enum Group {
    Gid(libc::gid_t),
    Name(String),
}

impl Group {
    fn current() -> Self {
        Group::Gid(c::linux::getgid())
    }

    fn getgr(&self) -> Res<c::linux::GetgrGroup> {
        let group = match self {
            Group::Gid(gid) => c::linux::getgrgid_r(*gid),
            Group::Name(name) => {
                let name = ffi::CString::new(name.as_str())
                    .expect("cannot convert group name");
                c::linux::getgrnam_r(&name)
            },
        }.map_err(|e| Error::Gid(e, self.clone()))?;
        group.ok_or_else(|| Error::GidNotFound(self.clone()))
    }
}

impl fmt::Display for Group {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Group::Gid(gid) => gid.fmt(f),
            Group::Name(name) => name.fmt(f),
        }
    }
}

impl FromStr for Group {
    type Err = GroupParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let gid = s.parse();
        match gid {
            Ok(gid) => Ok(Group::Gid(gid)),
            _ => Ok(Group::Name(s.into())),
        }
    }
}

pub struct GroupParseError { }

/// Wrapper around std::os::unix::net::UnixListener which unlinks the socket's
/// file when dropped, if not None.
#[derive(Debug)]
pub struct SocketFile {
    listener: unet::UnixListener,
    path: Option<path::PathBuf>,
}

impl SocketFile {
    fn new(listener: unet::UnixListener, path: Option<&path::Path>) -> Self {
        Self {
            listener,
            path: path.map(|p| p.into()),
        }
    }
}

impl Drop for SocketFile {
    fn drop(&mut self) {
        if let Some(ref path) = self.path {
            fs::remove_file(path).unwrap_or(());
        }
    }
}

/// The directory to place the program's runtime files in.
///
/// It contains a PID file `pid` which acts as a lock of the directory,
/// containing the currently holding process's PID, thereby ensuring that only
/// one process of this program may hold the directory at one time.
pub struct ProgramDir {
    dir: path::PathBuf,
}

impl ProgramDir {
    /// Try to acquire the program directory, creating it if needed.
    ///
    /// If the PID file exists with a PID naming a running process, the
    /// directory cannot be acquired; if it exists with a PID naming a
    /// non-running process, or does not exist at all, the directory is acquired
    /// via overwriting it with this process's PID.
    fn acquire() -> Res<Self> {
        const DIR_NAME: &'static str = crate_name!();
        let xdg_dirs = xdg::BaseDirectories::new()?;
        let dir = if xdg_dirs.has_runtime_directory() {
            xdg_dirs.create_runtime_directory(path::Path::new(DIR_NAME))?
        } else {
            const RUNTIME_DIR: &'static str = "/run";
            let dir = path::Path::new(RUNTIME_DIR)
                .join(path::Path::new(DIR_NAME));
            fs::create_dir_all(&dir)?;
            dir
        };

        let pid_file = Self::pid_file(&dir);
        match fs::OpenOptions::new().read(true).write(true).open(&pid_file) {
            Ok(mut file) => {
                // exists
                let mut buffer = [0; 128];
                let len = file.read(&mut buffer)?;
                if file.read(&mut buffer)? != 0 {
                    return Err(Error::PidFileSize(pid_file, buffer.len()));
                }
                let pid = String::from_utf8(Vec::from(&buffer[0..len]))?;
                let pid_dir = path::Path::new("/proc")
                    .join(path::Path::new(&pid));
                if pid_dir.exists() {
                    // the process in the PID file is running -- cannot acquire
                    return Err(Error::ProgramDirAcquire(dir));
                }
                // the process in the PID file is not running -- acquire by
                // writing current PID
                file.seek(io::SeekFrom::Start(0))?;
                file.set_len(0)?;
                let pid = process::id().to_string();
                file.write_all(pid.as_bytes())?;
            },
            Err(ref e) if e.kind() == io::ErrorKind::NotFound => {
                // does not exist -- acquire by creating and writing current PID
                let mut file = fs::OpenOptions::new().write(true)
                    .create_new(true).open(pid_file)?;
                let pid = process::id().to_string();
                file.write_all(pid.as_bytes())?;
            },
            Err(e) => return Err(e.into()),
        }

        Ok(Self {
            dir,
        })
    }

    fn pid_file(dir: &path::Path) -> path::PathBuf {
        const FILE_NAME: &'static str = "pid";
        dir.join(path::Path::new(FILE_NAME))
    }

    pub fn join(&self, path: &path::Path) -> path::PathBuf {
        self.dir.join(path)
    }
}

impl Drop for ProgramDir {
    /// Remove the PID file and the directory, no longer holding it.
    ///
    /// If the program exits abnormally and the `ProgramDir` object not dropped
    /// properly, the PID-file remains with an PID naming a non-running process.
    /// This is fine, as `acquire()` checks for this case.
    fn drop(&mut self) {
        let pid_file = Self::pid_file(&self.dir);
        if let Err(e) = fs::remove_file(&pid_file) {
            eprintln!("warning: PID file {:?} cannot be removed: {}",
                      pid_file, e);
        };
        if let Err(e) = fs::remove_dir(&self.dir) {
            eprintln!("warning: program dir {:?} cannot be removed: {}",
                      self.dir, e);
        }
    }
}
