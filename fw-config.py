# Important: This file is now exec() in the global scope
# Notes:
# - Enabling netplay in an emulator increases its size by ~350KB (~450KB in esp-idf 4.0)
# - Enabling profiling in an emulator increases its size by ~75KB (without no-inline)
# - Keep at least 32KB free in a partition for future updates
# - Partitions must be 64K aligned

PROJECT_NAME = "Retro-Go"
PROJECT_VER  = shell_exec("git describe --tags --abbrev=5 --dirty --always")
PROJECT_TILE = "fw-icon.raw"
PROJECT_APPS = {
  # Note: Size will be adjusted if needed but flashmon needs accurate values to work correctly
  # Project name   Sub, Size
  'retro-go':     [0,  393216],
  'nofrendo-go':  [0,  458752],
  'gnuboy-go':    [0,  393216],
  'smsplusgx-go': [0,  458752],
  'huexpress-go': [0,  458752],
  'handy-go':     [0,  458752],
  # 'meteor-go':    [0,  851968],
  # 'snes9x-go':    [0,  851968],
  # 'neopop-go':    [0,  720896],
  # 'mpython-go':   [0,  720896],
}
