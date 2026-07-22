# lightdm-tty

A terminal-style login theme for [web-greeter](https://github.com/JezerM/web-greeter).

Forked from [eNzyOfficial/lightdm-tty](https://github.com/eNzyOfficial/lightdm-tty), adapted from lightdm-webkit2-greeter to web-greeter.

## Screenshot

![Screenshot](https://github.com/eNzyOfficial/lightdm-tty/blob/master/screenshots/screenshot_1.gif)

## Installation

### 1. Install web-greeter (Arch Linux)

```bash
yay -S web-greeter
```

### 2. Install the theme

```bash
sudo mkdir -p /usr/share/web-greeter/themes/tty
sudo cp -r * /usr/share/web-greeter/themes/tty/
```

### 3. Configure greeter

Edit `/etc/lightdm/lightdm.conf`:
```ini
[Seat:*]
greeter-session=web-greeter
```

Edit `/etc/lightdm/web-greeter.toml`:
```toml
[greeter]
theme = "tty"
```

### 4. Test (before rebooting)

```bash
web-greeter --debug
```

### Testing in browser

You can test the theme locally by opening `index.html` in a browser. The `mock.js` provides fake data for preview.

## Commands

- `login [user]` — Authenticate as user
- `passwd` — Login to locked/current session
- `users` — List users
- `ls` — List available sessions (e.g. i3)
- `poweroff` — Shutdown
- `reboot` — Restart
- `suspend` — Suspend
- `clear` — Clear terminal
- `help` — List commands
- `man [command]` — Show help for command
- `motd` — Show MOTD

## Features

- Command history via ↑/↓
- Clear input via Ctrl+C
- Autocomplete via Tab

Lock with `dm-tool lock`.

## License

WTFPL — Do What The Fuck You Want To Public License.
