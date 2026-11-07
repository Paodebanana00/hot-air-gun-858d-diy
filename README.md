# hot-air-gun-858d-diy

Este projeto é o firmware completo para construir uma **Estação de Retrabalho de Ar Quente caseira baseada no cabo clássico da 858D**, controlada digitalmente por um **Raspberry Pi Pico (RP2040)**. 

O sistema gerencia o controle fino da rampa de temperatura (via PID e leitura do termopar MAX6675) e o chaveamento do Triac em corrente alternada, rodando de forma estável sobre o sistema operacional de tempo real **ChibiOS** com suporte a Dual-Core.

## 🛠️ Como Compilar e Instalar

### Pré-requisitos
Antes de começar, garanta que você tem as seguintes ferramentas instaladas no seu sistema (Linux/Ubuntu):
* **GCC ARM Embedded Toolchain** (`arm-none-eabi-gcc`)
* **Picotool** (ferramenta oficial da Raspberry Pi para gravar na Pico)
* **Python 3** com a biblioteca `pyserial` (usado para dar reboot automático via porta serial/CDC)

### 1. Clonar o Repositório
Não esqueça de puxar os submódulos do ChibiOS junto com o projeto:
```bash
git clone --recursive https://github.com
cd hot-air-gun-858d-diy
```

### 2. Compilar o Projeto
Para buildar o projeto e gerar o arquivo `.elf` e o binário `.uf2`:
```bash
make
```
*(O binário final será gerado dentro da pasta `./build`)*

### 3. Gravar na Placa (Flash)

**Método Automático (Via USB/CDC):**
Se a sua Pico já estiver rodando o firmware e conectada na porta USB (padrão `/dev/ttyACM0`), o comando abaixo envia um comando JSON para reiniciar a placa em modo BOOTSEL automaticamente e grava o arquivo:
```bash
make flash
```
*Se a sua porta USB for diferente, você pode passar o parâmetro:* `make flash CDC_PORT=/dev/ttyUSB0`



**Método Manual (Primeira Gravação):**
Se a placa estiver zerada ou se você colocou a Pico em modo BOOTSEL segurando o botão físico da placa ao conectar no PC, use o comando direto sem o reset via serial:
```bash
make flash1
```
