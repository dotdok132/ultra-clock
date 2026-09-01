#!/usr/bin/env bash
set -e

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}"
echo "  _  _ _ _             ___ _ lock"
echo " | || | | |_ _ _ __ _ / __| |_____ _| |__"
echo " | || | |  _| '_/ _\` | (__| / _ \ \ / / '_ \\"
echo "  \___/|_|\__|_| \__,_|\___|_|\___/_\_\_.__/"
echo -e "${NC}"
echo -e "${BOLD}The Absurdly Over-Engineered 100,000-Digit Atomic Clock Installer${NC}\n"

# Check dependencies
echo -e "${CYAN}[+] Checking dependencies...${NC}"

missing_pkgs=()

if ! command -v g++ &> /dev/null; then
    missing_pkgs+=("g++")
fi
if ! command -v make &> /dev/null; then
    missing_pkgs+=("make")
fi
if ! command -v pkg-config &> /dev/null; then
    missing_pkgs+=("pkg-config")
fi
if ! pkg-config --exists sdl2 2>/dev/null; then
    missing_pkgs+=("libsdl2-dev / SDL2")
fi

if [ ${#missing_pkgs[@]} -ne 0 ]; then
    echo -e "${RED}[!] Missing required dependencies:${NC} ${missing_pkgs[*]}"
    echo -e "${YELLOW}[i] Please install SDL2 development libraries and C++ build tools for your Linux distribution:${NC}"
    echo "    Ubuntu/Debian: sudo apt update && sudo apt install build-essential libsdl2-dev pkg-config"
    echo "    Arch Linux:    sudo pacman -S base-devel sdl2 pkgconf"
    echo "    Fedora:        sudo dnf groupinstall \"C Development Tools and Libraries\" && sudo dnf install SDL2-devel"
    exit 1
fi

echo -e "${GREEN}[✓] Dependencies satisfied!${NC}\n"

# Build project
echo -e "${CYAN}[+] Compiling UltraClock...${NC}"
make clean
make -j$(nproc 2>/dev/null || echo 2)

echo -e "\n${CYAN}[+] Installing UltraClock system-wide...${NC}"

# Check write access for /usr/local/bin
if [ -w "/usr/local/bin" ]; then
    make install PREFIX=/usr/local
else
    if command -v sudo &> /dev/null; then
        echo -e "${YELLOW}[i] Escalating privileges with sudo to install into /usr/local/bin...${NC}"
        sudo make install PREFIX=/usr/local
    else
        echo -e "${YELLOW}[i] Installing into user directory (~/.local/bin)...${NC}"
        mkdir -p ~/.local/bin ~/.local/share/applications
        make install PREFIX=~/.local
    fi
fi

echo -e "\n${GREEN}${BOLD}======================================================================${NC}"
echo -e "${GREEN}${BOLD}  ✓ UltraClock Installation Completed Successfully!${NC}"
echo -e "${GREEN}${BOLD}======================================================================${NC}"
echo -e "  • Launch from terminal:   ${CYAN}ultra_clock${NC}"
echo -e "  • Take 100,000-digit log: ${CYAN}ultra_clock --snapshot${NC}"
echo -e "  • Desktop Launcher:      Installed into Applications Menu"
echo -e "\n${YELLOW}Enjoy the most absurdly precise atomic clock ever built!${NC}\n"
