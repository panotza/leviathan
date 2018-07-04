mod none;
mod systemd;

use error::Res;

arg_enum!{
    #[derive(Debug)]
    pub enum InitSystemKind {
        None,
        Systemd,
    }
}

impl InitSystemKind {
    pub fn variant_default() -> Self {
        InitSystemKind::None
    }

    pub fn to_init_system<F, G>(&self, daemonize: F, socket_file_group: G) ->
        Res<Box<InitSystem>>
    where F: FnOnce() -> Res<Option<bool>>,
          G: FnOnce() -> Res<::Group>
    {
        Ok(match self {
            InitSystemKind::None => {
                Box::from(none::InitSystem::new(daemonize()?,
                                                socket_file_group()?))
            },
            InitSystemKind::Systemd => {
                Box::from(systemd::InitSystem::new(daemonize()?)?)
            },
        })
    }
}

pub trait InitSystem {
    fn daemonize(&self) -> bool;
    fn notify(&self) -> &Notify;
    fn socket_file(&self, program_dir: &::ProgramDir) -> Res<::SocketFile>;
}

pub trait Notify {
    fn startup_end(&self) -> Res<()>;
    fn reload_start(&self) -> Res<()>;
    fn reload_end(&self) -> Res<()>;
    fn shutdown_start(&self) -> Res<()>;
}
