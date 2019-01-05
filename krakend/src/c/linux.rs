use error::Errno;
use libc;
use std::{ffi, mem, path, ptr, str};
use std::os::unix::ffi::OsStrExt;

pub fn chmod(path: &path::Path, mode: ChmodMode) -> Result<(), Errno> {
    let path = path.as_os_str().as_bytes().as_ptr() as *const libc::c_char;
    let res = unsafe {
        // NOTE: [unsafe] OK, all arguments are validated
        libc::chmod(path, mode.into())
    };
    match res {
        0 => Ok(()),
        _ => Err(Errno::from_global_errno()),
    }
}

pub struct ChmodMode {
    pub group_execute: bool,
    pub group_read: bool,
    pub group_write: bool,
    pub other_execute: bool,
    pub other_read: bool,
    pub other_write: bool,
    pub owner_execute: bool,
    pub owner_read: bool,
    pub owner_write: bool,
    pub set_gid: bool,
    pub set_uid: bool,
    pub sticky: bool,
}

impl str::FromStr for ChmodMode {
    type Err = ChmodModeParseError;

    /// Convert a string of 12 characters in the format "ugsrwxrwxrwx" to a
    /// mode.
    ///
    /// The characters represent Set-UID, set-GID, sticky, and read, write,
    /// execute for user, group and other, in that order.  Each permission is
    /// considered false if the character is '-', true otherwise.
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let chars: Vec<_> = s.chars().collect();
        const PERMISSIONS: usize = 12;
        if chars.len() != PERMISSIONS {
            return Err(ChmodModeParseError { });
        }
        const PERMISSION_FALSE: char = '-';
        Ok(Self {
            set_uid:       (chars[0]  != PERMISSION_FALSE),
            set_gid:       (chars[1]  != PERMISSION_FALSE),
            sticky:        (chars[2]  != PERMISSION_FALSE),
            owner_read:    (chars[3]  != PERMISSION_FALSE),
            owner_write:   (chars[4]  != PERMISSION_FALSE),
            owner_execute: (chars[5]  != PERMISSION_FALSE),
            group_read:    (chars[6]  != PERMISSION_FALSE),
            group_write:   (chars[7]  != PERMISSION_FALSE),
            group_execute: (chars[8]  != PERMISSION_FALSE),
            other_read:    (chars[9]  != PERMISSION_FALSE),
            other_write:   (chars[10] != PERMISSION_FALSE),
            other_execute: (chars[11] != PERMISSION_FALSE),
        })
    }
}

#[derive(Debug)]
pub struct ChmodModeParseError { }

impl From<ChmodMode> for libc::mode_t {
    fn from(mode: ChmodMode) -> Self {
        (if mode.other_execute { 0b0000_0000_0001 } else { 0 } |
         if mode.other_write   { 0b0000_0000_0010 } else { 0 } |
         if mode.other_read    { 0b0000_0000_0100 } else { 0 } |
         if mode.group_execute { 0b0000_0000_1000 } else { 0 } |
         if mode.group_write   { 0b0000_0001_0000 } else { 0 } |
         if mode.group_read    { 0b0000_0010_0000 } else { 0 } |
         if mode.owner_execute { 0b0000_0100_0000 } else { 0 } |
         if mode.owner_write   { 0b0000_1000_0000 } else { 0 } |
         if mode.owner_read    { 0b0001_0000_0000 } else { 0 } |
         if mode.sticky        { 0b0010_0000_0000 } else { 0 } |
         if mode.set_gid       { 0b0100_0000_0000 } else { 0 } |
         if mode.set_uid       { 0b1000_0000_0000 } else { 0 })
    }
}

pub fn chown(path: &path::Path, owner: ChownUid, group: ChownGid) ->
    Result<(), Errno>
{
    let path = path.as_os_str().as_bytes().as_ptr() as *const libc::c_char;
    let res = unsafe {
        // NOTE: [unsafe] OK, all arguments are validated
        libc::chown(path, owner.into(), group.into())
    };
    match res {
        0 => Ok(()),
        _ => Err(Errno::from_global_errno()),
    }
}

pub enum ChownUid {
    Uid(libc::uid_t),
    Unspecified,
}

impl From<ChownUid> for libc::uid_t {
    fn from(uid: ChownUid) -> Self {
        match uid {
            ChownUid::Uid(uid) => uid,
            ChownUid::Unspecified => Self::max_value(),
        }
    }
}

pub enum ChownGid {
    Gid(libc::gid_t),
    Unspecified,
}

impl From<ChownGid> for libc::gid_t {
    fn from(gid: ChownGid) -> Self {
        match gid {
            ChownGid::Gid(gid) => gid,
            ChownGid::Unspecified => Self::max_value(),
        }
    }
}

pub fn fork() -> Result<ForkPid, Errno> {
    let res = unsafe {
        // NOTE: [unsafe] OK, fork() is always safe to call
        libc::fork()
    };
    match res {
        -1 => Err(Errno::from_global_errno()),
        0 => Ok(ForkPid::Parent),
        pid => Ok(ForkPid::Child(pid)),
    }
}

pub enum ForkPid {
    Child(libc::pid_t),
    Parent,
}

pub fn getgid() -> libc::gid_t {
    let gid = unsafe {
        // NOTE: [unsafe] OK, getgid() is always successful
        libc::getgid()
    };
    gid
}

pub fn getgrgid_r(gid: libc::gid_t) -> Result<Option<GetgrGroup>, Errno> {
    GetgrGroup::from_getgr_r(|grp, buf, buflen, result| {
        unsafe {
            // NOTE: [unsafe] OK, GetgrGroup::from_getgr_r makes sure to
            // validate all arguments
            libc::getgrgid_r(gid, grp, buf, buflen, result)
        }
    })
}

pub fn getgrnam_r(name: &ffi::CStr) -> Result<Option<GetgrGroup>, Errno> {
    GetgrGroup::from_getgr_r(|grp, buf, buflen, result| {
        unsafe {
            // NOTE: [unsafe] OK, see reason in getgrgid_r()
            libc::getgrnam_r(name.as_ptr(), grp, buf, buflen, result)
        }
    })
}

pub struct GetgrGroup {
    group: libc::group,
    // NOTE: The buffer stores the string fields of `group`, so we must retain
    // ownership of it as long as we have ownership of `group`.
    _buffer: Box<[libc::c_char]>,
}

impl GetgrGroup {
    fn from_getgr_r<F>(mut getgr_r: F) -> Result<Option<Self>, Errno>
    where F: FnMut(*mut libc::group, *mut libc::c_char, libc::size_t,
                   *mut *mut libc::group) -> libc::c_int
    {
        let mut group: libc::group = unsafe {
            // NOTE: [unsafe] OK, for if group is returned then it was
            // successfully filled by getgr_r and therefore has valid contents
            mem::uninitialized()
        };
        let mut result: *mut libc::group = ptr::null_mut();
        let mut buffer = {
            const CAPACITY_DEFAULT: usize = 128;
            let capacity = unsafe {
                // NOTE: [unsafe] OK, sysconf() handles any value safely
                libc::sysconf(libc::_SC_GETGR_R_SIZE_MAX)
            };
            let capacity = match capacity {
                -1 => CAPACITY_DEFAULT,
                size => size as usize,
            };
            Vec::with_capacity(capacity)
        };

        loop {
            let res = getgr_r(&mut group, buffer.as_mut_ptr(),
                              buffer.capacity(), &mut result);
            match res {
                0 => break,
                libc::ERANGE => {
                    let capacity_new = buffer.capacity() * 2;
                    buffer.reserve(capacity_new);
                    continue;
                },
                errno => return Err(Errno::from(errno)),
            };
        }
        if result.is_null() {
            return Ok(None);
        }

        Ok(Some(Self {
            group,
            _buffer: buffer.into_boxed_slice(),
        }))
    }

    #[allow(dead_code)]
    pub fn name(&self) -> &ffi::CStr {
        unsafe {
            // NOTE: [unsafe] OK, gr_name points into the buffer, which has the
            // lifetime of self
            ffi::CStr::from_ptr(self.group.gr_name)
        }
    }

    #[allow(dead_code)]
    pub fn password(&self) -> &ffi::CStr {
        unsafe {
            // NOTE: [unsafe] OK, see reason in name()
            ffi::CStr::from_ptr(self.group.gr_passwd)
        }
    }

    pub fn gid(&self) -> libc::gid_t {
        self.group.gr_gid
    }

    #[allow(dead_code)]
    pub fn group_members(&self) -> GetgrGroupMembers {
        GetgrGroupMembers {
            _group: self,
            mem_next: self.group.gr_mem,
        }
    }
}

pub struct GetgrGroupMembers<'a> {
    _group: &'a GetgrGroup,
    mem_next: *const *mut libc::c_char,
}

impl<'a> Iterator for GetgrGroupMembers<'a> {
    type Item = &'a ffi::CStr;

    fn next(&mut self) -> Option<Self::Item> {
        let mem_ref = unsafe {
            // NOTE: [unsafe] OK, mem_next points to valid memory in the buffer
            self.mem_next.as_ref()
        };
        match mem_ref {
            None => None,
            Some(member) => {
                self.mem_next = unsafe {
                    // NOTE: [unsafe] OK, mem_next always points to valid memory
                    // in the buffer as long as it's not incremented if it's
                    // NULL
                    self.mem_next.add(1)
                };
                let member = unsafe {
                    // NOTE: [unsafe] OK, *member points into the buffer
                    ffi::CStr::from_ptr(*member)
                };
                Some(member)
            },
        }
    }
}
