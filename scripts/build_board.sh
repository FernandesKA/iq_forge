#!/usr/bin/env bash
#
# Полная сборка платы: iq_forge_hdl (битстрим) -> buildroot_custom (образ
# Linux) -> iq_forge_fw (кросс-собранный CLI + деплой-архив), с раскладкой
# артефактов по нужным местам на каждом шаге:
#
#   1. iq_forge_hdl/build.sh <platform>
#        -> vivado/<platform>/iq_forge_hdl.runs/impl_1/{*.bit,*.bin,*_swapped.bin,*.xsa}
#   2. Свежий битстрим/xsa раскладываются туда, где их ждут следующие шаги:
#        - iq_forge_fw/configs/<board>/*.bit       (нужен deploy.sh)
#        - buildroot_external/board/zynq/<board-dir>/fpga/system.bit.bin
#          и .../system_wrapper.xsa                (только для плат с
#          загрузкой PL через U-Boot preboot, см. BOOT_BITSTREAM ниже)
#   3. buildroot_custom: make <defconfig> && make  -> output_<board>/...
#        (даёт кросс-тулчейн для шага 4 и, если плата того требует, сам
#        SD-образ с уже прошитым битстримом)
#   4. iq_forge_fw/scripts/deploy.sh (CC/CXX из свежесобранного тулчейна)
#        -> iq_forge_fw/dist/<board>.tar.gz
#   5. Все итоговые артефакты копируются в dist/<board>/{hdl,buildroot,fw}/
#      в этом репозитории.
#
# Usage: см. -h/--help
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

HDL_DIR="$REPO_ROOT/iq_forge_hdl"
FW_DIR="$REPO_ROOT/iq_forge_fw"
BR_DIR="$REPO_ROOT/buildroot_custom"
BR_ROOT="$BR_DIR/buildroot"
BR_EXTERNAL="$BR_DIR/buildroot_external"

# --- Таблица плат -----------------------------------------------------
# hdl-платформа (iq_forge_hdl/platforms/<..>), конфиг-папка fw
# (iq_forge_fw/configs/<..>), defconfig и board-dir buildroot, и флаг --
# нужна ли плате прошивка PL через U-Boot preboot (system.bit.bin,
# запекается в SD-образ) или PL поднимается только в рантайме через
# iq_forge_app (fpga_manager + overlay).
declare -A HDL_PLATFORM=(
    [rk7020f]="rk7020f"
    [pluto_sky]="pluto_sky"
)
declare -A FW_CONFIG=(
    [rk7020f]="rk7020f"
    [pluto_sky]="pluto_sky"
)
declare -A BR_DEFCONFIG=(
    [rk7020f]="zynq_rk7020f_iqforge_defconfig"
    [pluto_sky]="zynq_pluto_sky_defconfig"
)
declare -A BR_OUTPUT_DIR=(
    [rk7020f]="output_rk7020f_iqforge"
    [pluto_sky]="output_pluto_sky"
)
declare -A BR_BOARD_DIR=(
    [rk7020f]="RK-ZYNQ7020-F-IQFORGE"
    [pluto_sky]="pluto_sky"
)
declare -A BOOT_BITSTREAM=(
    [rk7020f]=1
    [pluto_sky]=0
)

# --- Аргументы ----------------------------------------------------------

show_usage() {
    cat <<EOF
Usage: $0 <board> [options]

Платы: $(printf '%s ' "${!HDL_PLATFORM[@]}")

Options:
  --skip-hdl         Не пересобирать битстрим в Vivado, взять уже собранный
                      из vivado/<platform>/iq_forge_hdl.runs/impl_1/
  --skip-buildroot   Не пересобирать образ Linux, взять уже собранный
                      тулчейн/образ из buildroot/<output-dir>/
  --skip-fw          Не пересобирать/не деплоить iq_forge_app
  -j, --jobs <N>     Параллелизм для Vivado и Buildroot [по умолчанию: nproc]
  --dry-run          Только напечатать команды, ничего не выполнять
  -h, --help         Эта справка

Examples:
  $0 rk7020f
  $0 pluto_sky --skip-buildroot -j 8
  $0 rk7020f --dry-run
EOF
    exit 0
}

BOARD=""
SKIP_HDL=0
SKIP_BUILDROOT=0
SKIP_FW=0
JOBS="$(nproc 2>/dev/null || echo 4)"
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-hdl)
            SKIP_HDL=1
            shift
            ;;
        --skip-buildroot)
            SKIP_BUILDROOT=1
            shift
            ;;
        --skip-fw)
            SKIP_FW=1
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            show_usage
            ;;
        -*)
            echo "Error: unknown option $1" >&2
            show_usage
            ;;
        *)
            if [[ -n "$BOARD" ]]; then
                echo "Error: unexpected extra argument '$1'" >&2
                exit 1
            fi
            BOARD="$1"
            shift
            ;;
    esac
done

if [[ -z "$BOARD" ]]; then
    echo "Error: missing <board>" >&2
    show_usage
fi
if [[ -z "${HDL_PLATFORM[$BOARD]+x}" ]]; then
    echo "Error: unknown board '$BOARD'. Known: ${!HDL_PLATFORM[*]}" >&2
    exit 1
fi

PLATFORM="${HDL_PLATFORM[$BOARD]}"
FWCFG="${FW_CONFIG[$BOARD]}"
DEFCONFIG="${BR_DEFCONFIG[$BOARD]}"
OUTPUT_DIR="${BR_OUTPUT_DIR[$BOARD]}"
BOARD_DIR_NAME="${BR_BOARD_DIR[$BOARD]}"
HAS_BOOT_BITSTREAM="${BOOT_BITSTREAM[$BOARD]}"

# --- Хелперы --------------------------------------------------------------

log() {
    echo ""
    echo "==> $*"
}

run() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        printf '[dry-run]'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

# Ровно один файл по маске в директории, иначе явная ошибка с числом
# найденных совпадений (тот же принцип, что в iq_forge_fw/scripts/deploy.sh).
find_one() {
    local dir="$1" pattern="$2" desc="$3"
    shift 3
    local matches=()
    while IFS= read -r -d '' f; do
        matches+=("$f")
    done < <(find "$dir" -maxdepth 1 -name "$pattern" "$@" -print0 2>/dev/null)
    if [[ ${#matches[@]} -ne 1 ]]; then
        echo "error: expected exactly one $desc ('$pattern') in $dir, found ${#matches[@]}" >&2
        exit 1
    fi
    printf '%s\n' "${matches[0]}"
}

# --- 1. iq_forge_hdl: битстрим --------------------------------------------

log "[1/5] HDL: $PLATFORM"

if [[ "$SKIP_HDL" -eq 1 ]]; then
    echo "skip-hdl: не пересобираю, использую уже собранное"
else
    if [[ ! -f "$HDL_DIR/vivado/$PLATFORM/iq_forge_hdl.xpr" ]]; then
        (cd "$HDL_DIR" && run ./create_project.sh "$PLATFORM")
    fi
    (cd "$HDL_DIR" && run ./build.sh "$PLATFORM" "$JOBS")
fi

IMPL_DIR="$HDL_DIR/vivado/$PLATFORM/iq_forge_hdl.runs/impl_1"

if [[ "$DRY_RUN" -eq 1 && ! -d "$IMPL_DIR" ]]; then
    echo "[dry-run] (импл-директория ещё не существует, дальше пути артефактов будут placeholder'ами)"
    HDL_BIT="$IMPL_DIR/system_wrapper.bit"
    HDL_BIN="$IMPL_DIR/system_wrapper.bin"
    HDL_SWAPPED_BIN="$IMPL_DIR/system_wrapper_swapped.bin"
    HDL_XSA="$IMPL_DIR/system_wrapper.xsa"
else
    HDL_BIT="$(find_one "$IMPL_DIR" '*.bit' "битстрим (.bit)")"
    HDL_XSA="$(find_one "$IMPL_DIR" '*.xsa' "hardware platform (.xsa)")"
    HDL_SWAPPED_BIN="$(find_one "$IMPL_DIR" '*_swapped.bin' "byte-swapped .bin")"
    HDL_BIN="$(find_one "$IMPL_DIR" '*.bin' "чистый .bin (не byte-swapped)" ! -name '*_swapped.bin')"
fi

echo "  bit:          $HDL_BIT"
echo "  bin (plain):  $HDL_BIN"
echo "  bin (swapped):$HDL_SWAPPED_BIN"
echo "  xsa:          $HDL_XSA"

# --- 2. Раскладка свежих HDL-артефактов туда, где их ждут следующие шаги --

log "[2/5] Раскладка HDL-артефактов"

echo "-> iq_forge_fw/configs/$FWCFG/ (нужен deploy.sh: ровно один *.bit)"
FW_CONFIG_DIR="$FW_DIR/configs/$FWCFG"
if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[dry-run] rm -f $FW_CONFIG_DIR/*.bit (старые) && cp $HDL_BIT $FW_CONFIG_DIR/"
else
    find "$FW_CONFIG_DIR" -maxdepth 1 -name '*.bit' -delete
    cp "$HDL_BIT" "$FW_CONFIG_DIR/"
fi

if [[ "$HAS_BOOT_BITSTREAM" -eq 1 ]]; then
    BOARD_DIR="$BR_EXTERNAL/board/zynq/$BOARD_DIR_NAME"
    echo "-> $BOARD_DIR/fpga/system.bit.bin (U-Boot preboot 'fpga loadfs')"
    run mkdir -p "$BOARD_DIR/fpga"
    run cp "$HDL_BIN" "$BOARD_DIR/fpga/system.bit.bin"
    echo "-> $BOARD_DIR/system_wrapper.xsa (zynq-dtgen, PS devicetree)"
    run cp "$HDL_XSA" "$BOARD_DIR/system_wrapper.xsa"
else
    echo "($BOARD не грузит PL через U-Boot preboot -- pропускаю fpga/system.bit.bin и .xsa)"
fi

# --- 3. buildroot_custom: образ Linux + кросс-тулчейн ----------------------

log "[3/5] Buildroot: $DEFCONFIG -> $OUTPUT_DIR"

if [[ "$SKIP_BUILDROOT" -eq 1 ]]; then
    echo "skip-buildroot: не пересобираю, использую уже собранное"
else
    if [[ ! -f "$BR_ROOT/$OUTPUT_DIR/.config" ]]; then
        (cd "$BR_ROOT" && run make O="$OUTPUT_DIR" BR2_EXTERNAL="$BR_EXTERNAL" "$DEFCONFIG")
    fi
    (cd "$BR_ROOT" && run make O="$OUTPUT_DIR" -j"$JOBS")
fi

TOOLCHAIN_BIN="$BR_ROOT/$OUTPUT_DIR/host/bin"
CC_BIN="$TOOLCHAIN_BIN/arm-linux-gcc"
CXX_BIN="$TOOLCHAIN_BIN/arm-linux-g++"

if [[ "$SKIP_FW" -eq 0 && "$DRY_RUN" -eq 0 && ( ! -x "$CC_BIN" || ! -x "$CXX_BIN" ) ]]; then
    echo "error: кросс-тулчейн не найден ($CC_BIN / $CXX_BIN)." >&2
    echo "       Собери buildroot для этой платы (без --skip-buildroot) перед сборкой fw." >&2
    exit 1
fi

# --- 4. iq_forge_fw: кросс-сборка CLI + деплой-архив -----------------------

log "[4/5] iq_forge_fw: $FWCFG"

if [[ "$SKIP_FW" -eq 1 ]]; then
    echo "skip-fw: не пересобираю/не деплою"
else
    echo "CC=$CC_BIN"
    echo "CXX=$CXX_BIN"
    # build/ кеширует тулчейн при первой конфигурации CMake -- если до этого
    # тут собирали под хост или другой тулчейн, снести его нужно всегда,
    # иначе CC/CXX не подхватится (см. комментарий в deploy.sh).
    run rm -rf "$FW_DIR/build" "$FW_DIR/dist"
    (
        cd "$FW_DIR"
        export CC="$CC_BIN" CXX="$CXX_BIN"
        run ./scripts/deploy.sh --arch zynq "configs/$FWCFG"
    )
fi

# --- 5. Финальная раскладка в dist/<board>/ --------------------------------

log "[5/5] Сбор артефактов в dist/$BOARD/"

DIST="$REPO_ROOT/dist/$BOARD"
run mkdir -p "$DIST/hdl/reports" "$DIST/buildroot" "$DIST/fw"

if [[ "$DRY_RUN" -eq 0 ]]; then
    cp "$HDL_BIT" "$HDL_BIN" "$HDL_SWAPPED_BIN" "$HDL_XSA" "$DIST/hdl/" 2>/dev/null || true
    if [[ -d "$HDL_DIR/reports/$PLATFORM" ]]; then
        cp "$HDL_DIR/reports/$PLATFORM"/*.rpt "$DIST/hdl/reports/" 2>/dev/null || true
    fi

    BR_IMAGES="$BR_ROOT/$OUTPUT_DIR/images"
    if [[ -d "$BR_IMAGES" ]]; then
        for f in sdcard.img boot.vfat boot.bin u-boot.img uImage system.dtb; do
            [[ -f "$BR_IMAGES/$f" ]] && cp "$BR_IMAGES/$f" "$DIST/buildroot/"
        done
    fi

    if [[ -f "$FW_DIR/dist/$FWCFG.tar.gz" ]]; then
        cp "$FW_DIR/dist/$FWCFG.tar.gz" "$DIST/fw/"
    fi
else
    echo "[dry-run] cp {bit,bin,swapped-bin,xsa,reports,sdcard.img,...,fw-archive} -> $DIST/{hdl,buildroot,fw}/"
fi

log "Готово: $BOARD"
echo "$DIST/"
if [[ "$DRY_RUN" -eq 0 ]]; then
    find "$DIST" -type f | sort
fi
