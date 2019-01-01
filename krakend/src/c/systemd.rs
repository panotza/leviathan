use error::Errno;
use libc;
use std::{ffi, iter};
use std::os::unix::io as uio;

const SD_LISTEN_FDS_START: libc::c_int = 3;

#[link(name = "systemd")]
extern "C" {
    fn sd_is_socket(fd: libc::c_int, family: libc::c_int, type_: libc::c_int,
                    listening: libc::c_int) -> libc::c_int;
    fn sd_listen_fds(unset_environment: libc::c_int) -> libc::c_int;
    fn sd_notify(unset_environment: libc::c_int,
                 state: *const libc::c_char) -> libc::c_int;
}

pub fn is_socket(fd: uio::RawFd, family: AddressFamily, type_: SocketType,
                 listening: Listening) -> Result<bool, Errno> {
    let res = unsafe {
        // NOTE: [unsafe] OK, all arguments are validated
        sd_is_socket(fd.into(), family.into(), type_.into(), listening.into())
    };
    if res < 0 {
        Err(Errno::from(-res))
    } else if res == 0 {
        Ok(false)
    } else {
        Ok(true)
    }
}

#[derive(Clone, Copy)]
pub enum AddressFamily {
    Inet,
    Inet6,
    Local,
    Unix,
    Unspecified,
}

impl From<AddressFamily> for libc::c_int {
    fn from(family: AddressFamily) -> Self {
        match family {
            AddressFamily::Inet => libc::AF_INET,
            AddressFamily::Inet6 => libc::AF_INET6,
            AddressFamily::Local => libc::AF_LOCAL,
            AddressFamily::Unix => libc::AF_UNIX,
            AddressFamily::Unspecified => libc::AF_UNSPEC,
        }
    }
}

#[derive(Clone, Copy)]
pub enum SocketType {
    Dgram,
    Raw,
    Rdm,
    Seqpacket,
    Stream,
    Unspecified,
}

impl From<SocketType> for libc::c_int {
    fn from(type_: SocketType) -> Self {
        match type_ {
            SocketType::Dgram => libc::SOCK_DGRAM,
            SocketType::Raw => libc::SOCK_RAW,
            SocketType::Rdm => libc::SOCK_RDM,
            SocketType::Seqpacket => libc::SOCK_SEQPACKET,
            SocketType::Stream => libc::SOCK_STREAM,
            SocketType::Unspecified => 0,
        }
    }
}

#[derive(Clone, Copy)]
pub enum Listening {
    Listening,
    NotListening,
    Unspecified,
}

impl From<Listening> for libc::c_int {
    fn from(listening: Listening) -> Self {
        match listening {
            Listening::Listening => 1,
            Listening::NotListening => 0,
            Listening::Unspecified => -1,
        }
    }
}

pub fn listen_fds(unset_environment: bool) -> Result<ListenFds, Errno> {
    let unset_environment = if !unset_environment { 0 } else { 1 };
    let res = unsafe {
        // NOTE: [unsafe] OK, all arguments are validated
        sd_listen_fds(unset_environment)
    };
    if res < 0 {
        Err(Errno::from(-res))
    } else {
        Ok(ListenFds {
            next: SD_LISTEN_FDS_START,
            end: SD_LISTEN_FDS_START + res,
        })
    }
}

pub struct ListenFds {
    next: libc::c_int,
    end: libc::c_int,
}

impl iter::Iterator for ListenFds {
    type Item = uio::RawFd;

    fn next(&mut self) -> Option<Self::Item> {
        if self.next >= self.end {
            None
        } else {
            let fd = self.next.into();
            self.next += 1;
            Some(fd)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = (self.end - self.next) as usize;
        (len, Some(len))
    }
}

impl iter::ExactSizeIterator for ListenFds { }
impl iter::FusedIterator for ListenFds { }

pub fn notify(unset_environment: bool, state: NotifyState) ->
    Result<bool, Errno>
{
    let unset_environment = if !unset_environment { 0 } else { 1 };
    let state = state.vars_to_vals.into_iter()
        .map(|(var, val)| format!("{}={}\n", var, val))
        .collect::<String>();
    let state = ffi::CString::new(state)
        .expect("state is invalid C string");

    let res = unsafe {
        // NOTE: [unsafe] OK, all arguments are validated
        sd_notify(unset_environment, state.as_ptr())
    };
    if res < 0 {
        Err(Errno::from(-res))
    } else if res == 0 {
        Ok(false)
    } else {
        Ok(true)
    }
}

pub struct NotifyState<'a> {
    vars_to_vals: &'a [(&'a str, &'a str)],
}

impl<'a> From<&'a [(&'a str, &'a str)]> for NotifyState<'a> {
    fn from(vars_to_vals: &'a [(&'a str, &'a str)]) -> Self {
        NotifyState {
            vars_to_vals,
        }
    }
}
