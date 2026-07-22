#!/bin/bash
set -euo pipefail

# lightdm-tty 一键安装脚本
# 适用于 Arch Linux 物理机 / VM

echo "==> 1/3 安装 web-greeter..."

if ! pacman -Q web-greeter &>/dev/null; then
    if command -v yay &>/dev/null; then
        yay -S --noconfirm web-greeter
    elif command -v paru &>/dev/null; then
        paru -S --noconfirm web-greeter
    else
        echo "请先安装 yay 或 paru" >&2
        exit 1
    fi
else
    echo "     web-greeter 已安装，跳过"
fi

echo "==> 2/3 部署主题..."

TTY_DIR="/usr/share/web-greeter/themes/tty"
sudo mkdir -p "$TTY_DIR"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp -r "$SCRIPT_DIR"/{index.html,index.yml,css,js,img} "$TTY_DIR/"
echo "     主题已部署到 $TTY_DIR"

echo "==> 3/3 配置 lightdm..."

# 设置 greeter-session
if grep -q "^greeter-session=" /etc/lightdm/lightdm.conf; then
    sudo sed -i 's/^greeter-session=.*/greeter-session=web-greeter/' /etc/lightdm/lightdm.conf
else
    echo "greeter-session=web-greeter" | sudo tee -a /etc/lightdm/lightdm.conf > /dev/null
fi
echo "     greeter-session → web-greeter"

# 设置主题
if [ -f /etc/lightdm/web-greeter.toml ]; then
    if grep -q "^theme " /etc/lightdm/web-greeter.toml; then
        sudo sed -i 's/^theme = .*/theme = "tty"/' /etc/lightdm/web-greeter.toml
    else
        sudo sed -i '/^\[greeter\]/a theme = "tty"' /etc/lightdm/web-greeter.toml
    fi
fi
echo "     web-greeter 主题 → tty"

echo ""
echo "================ 安装完成 ================"
echo "重启即可看到 lightdm-tty 登录界面"
echo ""
echo "如果进不去：Ctrl+Alt+F2 切 TTY 恢复："
echo "  sudo sed -i 's/greeter-session=web-greeter/greeter-session=lightdm-gtk-greeter/' /etc/lightdm/lightdm.conf"
echo "=========================================="
