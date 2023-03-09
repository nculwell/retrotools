
pub struct Pixel { pub r: u8, pub g: u8, pub b: u8 }

const DISPLAY_W: usize = 400;
const DISPLAY_H: usize = 200;

pub struct Display {
    pixels: [Pixel; DISPLAY_W * DISPLAY_H],
}

pub trait Media {
    fn draw(&self);
}

pub struct Sdl {
    context: sdl2::Sdl,
    timer: sdl2::TimerSubsystem,
    video: sdl2::VideoSubsystem,
    event: sdl2::EventSubsystem,
}

impl Media for Sdl {
    fn draw(&self) {
    }
}

macro_rules! sdl_init {
    ( $sdl_context:ident, $subsystem:ident ) => {
        match $sdl_context.$subsystem() {
            Ok(system) => system,
            Err(e) => { return Err(format!("SDL2 {}: {}", stringify!($subsystem), e)); }
        }
    };
}

pub fn init() -> Result<Sdl, String> {
    let ctx = match sdl2::init() {
        Ok(context) => context,
        Err(e) => { return Err(format!("SDL2 init: {}", e)); }
    };
    let timer = sdl_init!(ctx, timer);
    let video = sdl_init!(ctx, video);
    let event = sdl_init!(ctx, event);
    let result =
        Sdl {
            context: ctx,
            timer: timer,
            video: video,
            event: event,
        }
    ;
    Ok(result)
}

