
PROJECT = hot-air-gun-858d-diy

CHIBIOS = $(HOME)/Downloads/hot-air-gun-858d-diy/ChibiOS_Projects/ChibiOS
CHIBIOS_CONTRIB = $(HOME)/Downloads/hot-air-gun-858d-diy/ChibiOS_Projects/ChibiOS-Contrib

CONFDIR = ./cfg
BUILDDIR = ./build
DEPDIR = $(BUILDDIR)/.dep

BOARDPATH = $(CHIBIOS)/os/hal/boards/RP_PICO_RP2040

MCU = cortex-m0plus
USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
USE_COPT =
USE_CPPOPT = -fno-rtti
USE_FPU = no
USE_LINK_GC = yes
USE_LTO = yes
USE_SMART_BUILD = yes
USE_VERBOSE_COMPILE = no

# Licença e startup RP2040
include $(CHIBIOS)/os/license/license.mk
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_rp2040.mk

# HAL e plataforma: o Contrib integra a HAL principal
include $(CHIBIOS_CONTRIB)/os/hal/hal.mk
include $(CHIBIOS_CONTRIB)/os/hal/ports/RP/RP2040/platform.mk

# RT-NIL e port ARMv6-M
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv6-M/compilers/GCC/mk/port_rp2.mk

# Placa
include $(BOARDPATH)/board.mk

# Evita adicionar novamente os drivers antigos que a plataforma
# RP2040 do Contrib substitui, conforme o Makefile original.
ALLCSRC := $(filter-out \
    $(CHIBIOS)/os/hal/ports/RP/LLD/ADCv1/hal_adc_lld.c \
    $(CHIBIOS)/os/hal/ports/RP/LLD/PWMv1/hal_pwm_lld.c \
    $(CHIBIOS)/os/hal/ports/RP/LLD/USBv1/hal_usb_lld.c, \
    $(ALLCSRC))

# Fontes da aplicação e das bibliotecas
CSRC = $(sort \
    $(ALLCSRC) \
    $(HALSRC_CONTRIB) \
    $(PLATFORMSRC_CONTRIB) \
    $(CHIBIOS)/os/hal/lib/streams/chprintf.c \
    main.c \
    c1_main.c \
    module/thermocouple.c \
    module/usbcfg.c \
    module/potentiometers.c\
    module/convert/potentiometer_converter.c)

CPPSRC = $(ALLCPPSRC)
ASMSRC = $(ALLASMSRC)
ASMXSRC = $(ALLXASMSRC)

# Inicialização e dual-core
UDEFS = -DCRT0_VTOR_INIT=1 \
        -DCRT0_EXTRA_CORES_NUMBER=1 \
        -DNDEBUG

UADEFS = -DCRT0_VTOR_INIT=1 \
         -DCRT0_EXTRA_CORES_NUMBER=1

ULIBDIR =
ULIBS =

# Diretórios de cabeçalhos
INCDIR = $(CONFDIR) \
         $(ALLINC) \
         $(HALINC_CONTRIB) \
         $(PLATFORMINC_CONTRIB) \
         $(CHIBIOS_CONTRIB)/os/hal/include \
         module \
         drives_max6675 \
         $(CHIBIOS)/os/hal/lib/streams

# Linker
LDSCRIPT = $(STARTUPLD)/RP2040_FLASH.ld

# Compilador e regras
RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk

include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

# Geração UF2
PICOTOOL ?= $(CURDIR)/pico-kit-compile/picotool

$(BUILDDIR)/$(PROJECT).uf2: $(BUILDDIR)/$(PROJECT).elf
	$(PICOTOOL) uf2 convert $< $@

.PHONY: uf2
uf2: $(BUILDDIR)/$(PROJECT).uf2