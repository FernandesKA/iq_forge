# IQ Forge

SDR-платформа на базе трансивера **AD9361** и **Xilinx Zynq-7020**: 

FPGA-логика LVDS-интерфейса, embedded Linux (Buildroot), консольная прошивка для
управления трансивером и десктопное GUI для приёма/передачи IQ.

Репозиторий — надстройка над четырьмя сабмодулями, каждый решает свою часть
стека:

| Сабмодуль | Что внутри |
|---|---|
| [`iq_forge_hdl`](iq_forge_hdl) | Vivado-проект: RTL LVDS TX/RX интерфейса к AD9361, DDS, block design и констрейны под каждую плату |
| [`iq_forge_fw`](iq_forge_fw) | Кросс-компилируемое CLI-приложение на таргете: поднимает FPGA-битстрим, накатывает device-tree overlay, конфигурирует AD9361 по SPI (no-OS драйвер) |
| [`buildroot_custom`](buildroot_custom) | Buildroot BR2_EXTERNAL: сборка Linux образа и U-Boot (включая `ps7_init` — тактирование PS7) под каждую плату |
| [`iq_forge_gui`](iq_forge_gui) | Десктопное приложение: TX-генератор, RX/спектр/водопад, работает с PlutoSDR и HackRF напрямую по сети/USB (пока не зависит от остального стека) |

Подробности по каждой части — в README соответствующего сабмодуля.

## Поддерживаемые платы

| Плата | FPGA | Buildroot defconfig | HDL-платформа |
|---|---|---|---|
| `rk7020f` | xc7z020clg484-2 | `zynq_rk7020f_iqforge_defconfig` | `rk7020f` |
| `pluto_sky` | xc7z020clg400-2 | `zynq_pluto_sky_defconfig` | `pluto_sky` |

Обе платы собираются на одинаковых клоках и одинаковой конфигурации регистров
AD9361 — конфиги (`dds_clk_hz` и т.п.) в `iq_forge_fw/configs/<плата>/`
специально держатся идентичными между платами.

## Архитектура (в общих чертах)

```
				AD9361 (SPI + LVDS) <-> PL (iq_forge_hdl: TX/RX LVDS, DDS)
                          |
                PS7 (Zynq) — Linux (buildroot_custom)
                          |
                iq_forge_app (iq_forge_fw) — грузит битстрим,
                применяет overlay, конфигурирует AD9361 по SPI
                          |
                iq_forge_gui (опционально, по сети/USB, пока что отдельно
                от Zynq-стека — работает с дефолтной прошивкой PlutoSDR/HackRF)
```

- **PL** формирует LVDS TX/RX к AD9361 и DDS-генератор тестового тона;
  клоки платы задаются в `iq_forge_hdl/platforms/<плата>/bd.tcl`
  (`PCW_FPGA0_PERIPHERAL_FREQMHZ`) и должны совпадать с
  `iq_forge_fw/configs/<плата>/dds_clk_hz`.
- **PS7** поднимает Linux; тактовые делители FCLK0 и остальная конфигурация
  SLCR зашиваются в `ps7_init_gpl.c/h`, который Vivado генерирует из того же
  `bd.tcl` и который нужно руками скопировать в
  `buildroot_custom/buildroot_external/board/zynq/<board>/ps7_init/` после
  любого изменения клоков в HDL — иначе U-Boot продолжит грузить плату на
  старых делителях.
- **iq_forge_app** — единственная точка входа на таргете: без аргументов сам
  подхватывает `manifest.env`/`spi.json` из своей директории и прогоняет всю
  цепочку (`fpga_manager` → overlay → инициализация AD9361 по SPI).

## Быстрый старт

### 1. Клонировать с сабмодулями

```sh
git clone --recurse-submodules git@github.com:FernandesKA/iq_forge.git
cd iq_forge
```

Если уже склонировано без сабмодулей:

```sh
git submodule update --init --recursive
```

### 2. Собрать плату целиком

`scripts/build_board.sh` прогоняет весь стек по порядку — HDL-битстрим →
Linux-образ (buildroot) → кросс-сборка и деплой-архив прошивки — и
раскладывает готовые артефакты по местам, где их ждёт следующий шаг:

```sh
./scripts/build_board.sh rk7020f
./scripts/build_board.sh pluto_sky
```

Полезные флаги (см. `--help`): `--skip-hdl` / `--skip-buildroot` / `--skip-fw`
— пропустить пересборку уже готового шага, `--dry-run` — только показать, что
будет выполнено, `-j <N>` — параллелизм для Vivado и Buildroot.

Итоговые артефакты собираются в `dist/<плата>/{hdl,buildroot,fw}/` (в git не
хранится, см. `.gitignore`).

### 3. Залить прошивку на плату

```sh
cd iq_forge_fw
./scripts/load.sh dist/rk7020f.tar.gz          # --start: /tmp, не переживает reboot
./scripts/load.sh --persistent dist/rk7020f.tar.gz   # переживает reboot
```

По умолчанию `load.sh` идёт на `192.168.0.7` (см. `--host`, если плата на
другом адресе) — правится под свое железо в `iq_forge_fw/scripts/load.sh`.

### 4. Собирать части по отдельности

Каждый сабмодуль можно собирать и итерировать независимо — см. README внутри:
`iq_forge_hdl/README.md` (`create_project.sh` / `build.sh`),
`buildroot_custom/README.md` (`make <defconfig> && make`),
`iq_forge_fw` (`scripts/deploy.sh` для кросс-сборки CLI и деплой-архива).

## GUI

`iq_forge_gui` пока что не завязан на остальной стек — работает напрямую с дефолтным софтом PlutoSDR/HackRF по сети/USB, TX-генератор, RX со спектром/водопадом. 

Сборка и
возможности — в `iq_forge_gui/README.md`.
