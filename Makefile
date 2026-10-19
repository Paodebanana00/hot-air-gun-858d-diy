# Projeto
PROJECT = teste_chibios

# Caminhos
CHIBIOS = $(HOME)/Downloads/hot-air-gun-858d-diy/ChibiOS_Projects/ChibiOS
CHIBIOS_CONTRIB = $(HOME)/Downloads/hot-air-gun-858d-diy/ChibiOS_Projects/ChibiOS-Contrib
CONFDIR = ./cfg
BUILDDIR = ./build
DEPDIR = $(BUILDDIR)/.dep

# Compilador
MCU = cortex-m0plus
USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
USE_COPT =
USE_CPPOPT = -fno-rtti
USE_FPU = no
USE_LINK_GC = yes
USE_LTO = yes
USE_SMART_BUILD = yes
USE_VERBOSE_COMPILE = no

# Inicialização
include $(CHIBIOS)/os/license/license.mk
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_rp2040.mk

# HAL e plataforma RP2040 do núcleo ChibiOS
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/RP/RP2040/platform.mk
include $(CHIBIOS)/os/hal/boards/RP_PICO_RP2040/board.mk

# Kernel
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv6-M/compilers/GCC/mk/port_rp2.mk

# Fontes
CSRC = $(ALLCSRC) \
       $(CHIBIOS_CONTRIB)/os/hal/src/hal_community.c \
       main.c \
       c1_main.c
CPPSRC = $(ALLCPPSRC)
ASMSRC = $(ALLASMSRC)
ASMXSRC = $(ALLXASMSRC)	

# Configuração
UDEFS = -DCRT0_VTOR_INIT=1 -DCRT0_EXTRA_CORES_NUMBER=1 -DNDEBUG
UADEFS = -DCRT0_VTOR_INIT=1 -DCRT0_EXTRA_CORES_NUMBER=1
UINCDIR =
ULIBDIR =
ULIBS =

INCDIR = $(CONFDIR) $(ALLINC) \
         $(HOME)/Downloads/hot-air-gun-858d-diy/ChibiOS_Projects/ChibiOS-Contrib/os/hal/include

# Linker
LDSCRIPT = $(STARTUPLD)/RP2040_FLASH.ld

# Regras de compilação
RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk
include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

# Geração do UF2
PICOTOOL ?= $(CURDIR)/pico-kit-compile/picotool

$(BUILDDIR)/$(PROJECT).uf2: $(BUILDDIR)/$(PROJECT).elf
	$(PICOTOOL) uf2 convert $< $@

.PHONY: uf2
uf2: $(BUILDDIR)/$(PROJECT).uf2

