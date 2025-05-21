#!/bin/bash
# [*] ZMK-Nix Setup Script
# [*] Color definitions
GREEN='\033[0;32m'
BLUE='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[1;36m'
PURPLE='\033[0;35m'
WHITE='\033[1;37m'
NC='\033[0m'

# [*] Process command line arguments
DRY_RUN=false
PRINT_DELAY=0.02

show_help() {
    cat << EOF
ZMK-Nix Setup Script - Usage:
    ./nixsetup.sh [options]

Options:
    --help      Show this help message
    --dry-run   Run without executing the final build step
                Also speeds up the text output

This script helps set up a ZMK firmware build environment using Nix.
It will analyze your build.yaml configuration and generate a flake.nix
file with the appropriate settings for your keyboard.
EOF
    exit 0
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --help) show_help ;;
        --dry-run) DRY_RUN=true; PRINT_DELAY=0.001 ;;
        *) echo "Unknown parameter: $1"; show_help ;;
    esac
    shift
done

# [*] Loading animation function
show_loading() {
    local pid=$1
    local delay=0.1
    local spinstr='|/-\'
    
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        local temp=${spinstr#?}
        printf "\r${CYAN}[%c]${NC} " "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
    done
    printf "\r"
}

# [*] Typewriter-style output
hack_print() {
    local text=$1
    local delay=${2:-$PRINT_DELAY}
    for ((i=0; i<${#text}; i++)); do
        echo -n "${text:$i:1}"
        sleep $delay
    done
    echo
}

# [*] ASCII art logo
display_logo() {
    echo -e "${CYAN}"
    cat << "EOF"
    ███████╗███╗   ███╗██╗  ██╗    ███╗   ██╗██╗██╗  ██╗
    ╚══███╔╝████╗ ████║██║ ██╔╝    ████╗  ██║██║╚██╗██╔╝
      ███╔╝ ██╔████╔██║█████╔╝     ██╔██╗ ██║██║ ╚███╔╝ 
     ███╔╝  ██║╚██╔╝██║██╔═██╗     ██║╚██╗██║██║ ██╔██╗ 
    ███████╗██║ ╚═╝ ██║██║  ██╗    ██║ ╚████║██║██╔╝ ██╗
    ╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝
                ZMK-Nix Build Environment
EOF
    echo -e "${NC}"
}

# [*] System information display
show_system_info() {
    # Get git information
    local git_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "N/A")
    local git_commit=$(git rev-parse --short HEAD 2>/dev/null || echo "N/A")

    echo -e "${PURPLE}╔════ SYSTEM INFORMATION ═══════════════════════╗${NC}"
    echo -e "${PURPLE}║${NC} ${CYAN}User:${NC}     $(whoami)"
    echo -e "${PURPLE}║${NC} ${CYAN}Date:${NC}     $(date '+%Y-%m-%d %H:%M:%S')"
    echo -e "${PURPLE}║${NC} ${CYAN}System:${NC}   $(uname -sr)"
    echo -e "${PURPLE}║${NC} ${CYAN}Branch:${NC}   $git_branch"
    echo -e "${PURPLE}║${NC} ${CYAN}Commit:${NC}   $git_commit"
    echo -e "${PURPLE}╚═══════════════════════════════════════════════╝${NC}"
    echo ""
}

# [*] Progress bar renderer
show_progress() {
    local current=$1
    local total=$2
    local width=40
    local percentage=$(( (current * 100) / total ))
    local completed=$(( (width * current) / total ))
    local remaining=$((width - completed))
    
    printf "\r${BLUE}[${CYAN}"
    printf '%*s' "$completed" | tr ' ' '#'
    printf '%*s' "$remaining" | tr ' ' '-'
    printf "${BLUE}] ${WHITE}%3d%%${NC}\n" "$percentage"
}

# [*] Parse build.yaml configuration
parse_build_yaml() {
    hack_print "Analyzing build.yaml configuration..." 0.03
    
    if [ ! -f "build.yaml" ]; then
        echo -e "${RED}[✗] ERROR: build.yaml not found in current directory${NC}"
        exit 1
    fi

    # [+] Parse shield configuration
    SHIELDS=()
    SNIPPETS=()
    while IFS= read -r line; do
        if [[ $line =~ shield:\ *(.*) ]]; then
            shield="${BASH_REMATCH[1]}"
            if [[ $shield != "settings_reset" ]]; then
                SHIELDS+=("$shield")
                echo -e "${BLUE}[*]${NC} Found shield: $shield"
            fi
        elif [[ $line =~ snippet:\ *(.*) ]]; then
            SNIPPETS+=("${BASH_REMATCH[1]}")
        fi
    done < build.yaml

    echo -e "\n${CYAN}=== Shield Configuration Analysis ===${NC}"
    echo -e "${BLUE}[*]${NC} Found ${#SHIELDS[@]} shield entries"

    # [+] Extract shield base name
    SHIELD_BASE=""
    IS_SPLIT=false
    if [ ${#SHIELDS[@]} -gt 0 ]; then
        IFS=' ' read -ra SHIELD_PARTS <<< "${SHIELDS[0]}"
        if [[ "${SHIELD_PARTS[0]}" =~ _[LR]$ ]]; then
            IS_SPLIT=true
            SHIELD_BASE=$(echo "${SHIELD_PARTS[0]}" | sed 's/_[LR]$//' | tr -d '\n')
        else
            SHIELD_BASE="${SHIELD_PARTS[0]}"
        fi
        echo -e "${GREEN}[✓]${NC} Shield base: $SHIELD_BASE"
        echo -e "${GREEN}[✓]${NC} Keyboard type: $([ "$IS_SPLIT" = true ] && echo "Split keyboard" || echo "Unified keyboard")"
    fi

    echo -e "\n${CYAN}=== Part Configuration Generation ===${NC}"
    # [+] Generate parts list
    declare -A unique_parts
    PARTS=()
    for shield in "${SHIELDS[@]}"; do
        echo -e "${BLUE}[*]${NC} Processing shield: $shield"
        IFS=' ' read -ra SHIELD_PARTS <<< "$shield"
        if [[ "${SHIELD_PARTS[0]}" =~ _([LR])$ ]]; then
            part="${BASH_REMATCH[1]}"
            if [ ! -z "$part" ] && [ -z "${unique_parts[$part]}" ]; then
                part_config="\"$part"
                if [ ${#SHIELD_PARTS[@]} -gt 1 ]; then
                    # Add extras based on part type
                    if [ "$part" = "L" ]; then
                        part_config+=" ${SHIELD_PARTS[@]:1}"
                    elif [ "$part" = "R" ] && [ "${SHIELD_PARTS[1]}" = "rgbled_adapter" ]; then
                        part_config+=" rgbled_adapter"
                    fi
                fi
                part_config+="\""
                unique_parts[$part]=1
                PARTS+=($part_config)
                echo -e "${GREEN}[✓]${NC} Generated part config: $part_config"
            fi
        fi
    done

    echo -e "${GREEN}[✓]${NC} Configuration loaded successfully"
    hack_print "Shield Base: $SHIELD_BASE" 0.02

    if [ ${#PARTS[@]} -gt 0 ]; then 
        hack_print "Parts: ${PARTS[*]}" 0.02
    else
        hack_print "No parts found in build.yaml" 0.02
    fi
    hack_print "Snippets: ${SNIPPETS[*]:-zmk-usb-logging}" 0.02
}

# [*] Nix environment verification
check_nix() {
    if ! command -v nix &> /dev/null; then
        echo -e "${YELLOW}[!] Nix not found. Installing...${NC}"
        curl -L https://nixos.org/nix/install | sh
        . ~/.nix-profile/etc/profile.d/nix.sh
    fi
    echo -e "${GREEN}[✓]${NC} Nix installation verified"
}

# [*] Generate flake.nix configuration
generate_flake_nix() {
    # [+] Convert arrays to config strings
    local parts_config=""
    local snippets_config=""
    local studio_config=""
    
    if [ ${#PARTS[@]} -gt 0 ]; then
        # Join parts with spaces and remove any newlines
        local parts_str=$(printf "%s " "${PARTS[@]}" | tr -d '\n\r' | sed 's/ $//')
        echo -e "${GREEN}[✓]${NC} Configured parts: $parts_str"
        parts_config="        parts = [ $parts_str ];"
    fi
    
    # Filter out studio-rpc-usb-uart from snippets and check if it exists
    local has_studio=false
    declare -A unique_snippets

    # Debug output for raw snippets
    echo -e "${BLUE}[*]${NC} Raw snippets: ${SNIPPETS[*]}"
    
    # Split and process snippets
    IFS=' ' read -ra SPLIT_SNIPPETS <<< "${SNIPPETS[*]}"
    for single_snippet in "${SPLIT_SNIPPETS[@]}"; do
        # Remove any carriage returns and trim whitespace
        single_snippet=$(echo "$single_snippet" | tr -d '\r' | xargs)
        
        echo -e "${BLUE}[*]${NC} Processing snippet: $single_snippet"
        if [ "$single_snippet" = "studio-rpc-usb-uart" ]; then
            echo -e "${YELLOW}[!]${NC} Found studio-rpc-usb-uart snippet"
            has_studio=true
        else
            unique_snippets["$single_snippet"]=1
        fi
    done

    # Convert unique snippets to array
    filtered_snippets=()
    for snippet in "${!unique_snippets[@]}"; do
        filtered_snippets+=("\"$snippet\"")
    done

    if [ ${#filtered_snippets[@]} -gt 0 ]; then
        local snippets_str=$(printf "%s," "${filtered_snippets[@]}" | sed 's/,$//')
        snippets_config="        snippets = [ $snippets_str ];"
        echo -e "${GREEN}[✓]${NC} Configured snippets (deduplicated): $snippets_str"
    fi

    if [ "$has_studio" = true ]; then
        studio_config="        enableZmkStudio = true;"
    fi

    cat > flake.nix << EOF
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    zmk-nix = {
      url = "github:lilyinstarlight/zmk-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, zmk-nix }: let
    forAllSystems = nixpkgs.lib.genAttrs (nixpkgs.lib.attrNames zmk-nix.packages);
  in {
    packages = forAllSystems (system: rec {
      default = firmware;

      firmware = zmk-nix.legacyPackages.\${system}.build$([ "$IS_SPLIT" = true ] && echo "Split")Keyboard {
        name = "firmware";
        src = nixpkgs.lib.sourceFilesBySuffices self [ ".board" ".cmake" ".conf" ".defconfig" ".dts" ".dtsi" ".json" ".keymap" ".overlay" ".shield" ".yml" "_defconfig" ];
        board = "seeeduino_xiao_ble";
        shield = "${SHIELD_BASE}_%PART%";
$([ -n "$parts_config" ] && echo "$parts_config")
$([ -n "$snippets_config" ] && echo "$snippets_config")
$([ -n "$studio_config" ] && echo "$studio_config")
        zephyrDepsHash = "sha256-YkNPlLZcCguSYdNGWzFNfZbJgmZUhvpB7DRnj++XKqQ=";
        meta = {
          description = "ZMK firmware";
          license = nixpkgs.lib.licenses.mit;
          platforms = nixpkgs.lib.platforms.all;
        };
      };

      flash = zmk-nix.packages.\${system}.flash.override { inherit firmware; };
      update = zmk-nix.packages.\${system}.update;
    });

    devShells = forAllSystems (system: {
      default = zmk-nix.devShells.\${system}.default;
    });
  };
}
EOF
}

# [*] Extract hash from error output
extract_hash() {
    echo "$1" | grep "got:" | awk '{print $2}'
}

# [*] Main execution flow
main() {
    clear
    display_logo
    show_system_info
    
    total_steps=$([ "$DRY_RUN" = true ] && echo "4" || echo "5")

    # [*] Verify Nix environment
    echo -e "\n${BLUE}[*]${NC} Checking Nix installation..."
    show_progress 1 $total_steps
    check_nix
    echo ""

    # [*] Analyzing build configuration
    echo -e "${BLUE}[*]${NC} Analyzing build configuration..."
    show_progress 2 $total_steps
    parse_build_yaml

    # [*] Init ZMK-Nix template
    hack_print "Initializing ZMK-Nix template..." 0.02
    show_progress 3 $total_steps
    nix flake init --template github:lilyinstarlight/zmk-nix

    # [*] Generate flake.nix
    hack_print "Generating flake.nix configuration..." 0.02
    show_progress 4 $total_steps
    generate_flake_nix

    # [*] Execute firmware build
    if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}[!]${NC} Dry run mode - skipping build"
        hack_print "Build would be performed here in normal mode" 0.02
    else
        hack_print "Building firmware..." 0.02
        show_progress 5 $total_steps
        if ! nix build -L 2> build_error.log; then
            error_output=$(cat build_error.log)
            if echo "$error_output" | grep -q "hash mismatch"; then
                echo -e "${YELLOW}[!] Hash mismatch detected. Attempting to fix...${NC}"
                new_hash=$(extract_hash "$error_output")
                if [ ! -z "$new_hash" ]; then
                    sed -i "s|zephyrDepsHash = \".*\"|zephyrDepsHash = \"$new_hash\"|" flake.nix
                    hack_print "Updated hash: $new_hash" 0.02
                    hack_print "Retrying build..." 0.02
                    nix build -L
                fi
            else
                cat build_error.log
                exit 1
            fi
        fi
        rm -f build_error.log
    fi

    # [*] Success banner
    echo -e "\n${GREEN}┌────────────────────────────────────────┐${NC}"
    echo -e "${GREEN}│${NC}     🎉 Setup Complete! 🎉           ${GREEN}│${NC}"
    echo -e "${GREEN}└────────────────────────────────────────┘${NC}\n"

    hack_print "Build artifacts location: ./result/" 0.03
    echo -e "\n${CYAN}Next steps:${NC}"
    echo -e "1. Configure your keymap in ${WHITE}config/${NC} directory"
    echo -e "2. Commit and push your changes"
    echo -e "3. Run ${WHITE}nix build${NC} to rebuild firmware"
}

# [!] Execute main process
main "$@"