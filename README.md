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
make uf2
```
*(O binário final será gerado dentro da pasta `./build`)*

### 3. Gravar na Placa (Flash)

**Método Automático (Via USB/CDC):**
Se a sua Pico já estiver rodando o firmware e conectada na porta USB (padrão `/dev/ttyACM0`), o comando abaixo envia um comando JSON para reiniciar a placa em modo BOOTSEL automaticamente e grava o arquivo:
```bash
make flash
```
*Se a sua porta USB for diferente, você pode passar o parâmetro:* `make flash CDC_PORT=/dev/ttyUSB0`


## 📦 Lista de Componentes Básicos (BOM)

Para montar esta estação utilizando uma placa perfurada universal, você precisará dos seguintes componentes:

### 1. Elemento de Aquecimento e Sensores
* **Cabo de Reposição Estação 858D:** Modelo padrão de 8 pinos com ventoinha de 24V interna e termopar tipo K (fácil de encontrar no AliExpress).
* **Módulo MAX6675:** Placa de leitura de termopar pronta (comunicação SPI com o RP2040).
* **Display LCD 1602A com Módulo I2C:** Tela de 16 colunas por 2 linhas para interface visual.

### 2. Controle Digital
* **Raspberry Pi Pico (Padrão):** Versão convencional (sem Wi-Fi), baseada no microcontrolador RP2040.

### 3. Circuito de Potência (Resistência AC)
* **TRIAC BTA16:** Chaveamento principal da resistência de corrente alternada.
* **Optoacoplador MOC3041:** Isolamento elétrico com detecção interna de cruzamento por zero (*Zero-Cross*).
* **Transistor PNP (Acionamento do MOC):** Para acionamento seguro do opto em 3.3V a partir dos pinos da Pico.
* **Resistores do Bloco AC:**
  * 1x Resistor de $100\,\Omega$ (limitação de corrente na entrada do MOC).
  * 1x Resistor de $1\,\text{k}\Omega$ (base do transistor).
  * 1x Resistor de $4,7\,\text{k}\Omega$ (pull-up entre emissor e coletor).

### 4. Circuito do Cooler (Controle PWM de 24V) (esse circuito pode ser refeito ultilizando um mosfet aí é problema seu)
* **Transistor de Potência ultilizado C5027S:** Transistor principal de comutação para controlar a carga da ventoinha do cabo você pode alterar para que tiver em mão mas é necessario recalcular os resistores.
* **Transistor BC337:** Transistor NPN de sinal responsável por excitar a base do C5027S, pode ser subistituido para qualquer transsitor npn de baixa e media potencia respeitando os pinos dele.
* **Diodo Schottky SB2040CT:** Diodo duplo de alta velocidade utilizado como **Diodo de Roda-Livre** em paralelo com o motor da ventoinha para ceifar os picos indutivos, pode ser subistituido por qualquer diodo schottky que tiver em mãos.
* **Capacitor de Filtro:** 1x Capacitor Eletrolítico de $470\,\mu\text{F} \times 50\text{V}$ (como o pwm trabalha em mais ou menos 8khz pode por um capacitor ceramico de 10nf ou oq tiver que funciona perfeitamente) em paralelo com a alimentação da fan para filtragem de ruído do PWM.
* **Diodo 1N4148:** Diodo de sinal rápido ligado em série na base do BC337 para acionamento e proteção contra corrente reversa.
* **Resistores do Bloco PWM:**
  * 1x Resistor de $1\,\text{k}\Omega$ (em série com o diodo 1N4148 na malha de acionamento da base).
  * 1x Resistor de $4,7\,\text{k}\Omega$ (configurado como *Pull-Down* na base do BC337).
  * 2x Resistores de $47\,\Omega$ (limitação de corrente para os transistores).

### 5. Sistema de Alimentação (Sugestões de Montagem)
* **Versão Utilizada no Protótipo:** Fonte chaveada de 12V convencional, elevando a tensão para 24V através de um módulo Booster **XL6019**, além de uma linha secundária de 5V para o display/lógica.
* **Recomendação Ideal para Próximas Montagens:** Utilizar uma única **Fonte de 24V** nativa (para alimentar o cooler diretamente) combinada a um conversor **Step-Down** (ou regulador linear tipo o LM7805 com dissipador) para rebaixar e alimentar a linha de 5V para a tela o microcontrolador e a base do transistor de potencia

## 🛠️ Modificação Obrigatória: Display LCD 1602 I2C no RP2040

> ⚠️ **ATENÇÃO:** Os módulos adaptadores I2C comerciais (PCF8574) vêm com resistores de pull-up interligados nos 5V. Como a Raspberry Pi Pico tolera **apenas 3.3V**, ligar o módulo direto da fábrica vai injetar 5V nos pinos da Pico e **queimar o microcontrolador**. 

Siga o procedimento abaixo para fazer uma modificação híbrida: manter o brilho máximo no LCD e proteger a lógica em 3.3V.

### 1. Modificação Física no Módulo I2C (PCF8574)
* **Remova os Pull-ups de 5V:** Localize e remova completamente os dois resistores SMD com a inscrição **472** (4.7kΩ) que ficam próximos ao chip PCF8574. Encoste o ferro de solda deitado sobre eles para sacá-los da placa.
* Com isso, as linhas SDA e SCL do módulo ficam em modo Dreno Aberto puro, totalmente isoladas da tensão de 5V.

### 2. Nova Conexão de Alimentação e Dados
* **VCC do LCD:** Ligue direto no pino **VBUS (5V)** da Pico ou na linha de **4.8V/5V** da sua fonte principal. (Isso garante contraste e brilho máximo nos dígitos brancos).
* **GND do LCD:** Ligue no GND comum do circuito.
* **Linhas de Dados:** Conecte o **SDA no GP18** e o **SCL no GP19** (Barramento I2C0). 
  * *Nota de projeto:* Usar essa extremidade inferior afasta os fios sensíveis da fiação de potência do Triac e do cooler, mitigando interferências eletromagnéticas (EMI).

### 3. Instalação dos Pull-ups Externos em 3.3V 
* Pegue dois resistores comuns de terminal de **4.7kΩ** (ou reaproveite os SMDs se tiver as manhas) e instale-os externamente na placa perfurada:
  * Um resistor ligando a linha **SDA (GP18) ao pino de 3.3V** da Pico.
  * Um resistor ligando a linha **SCL (GP19) ao pino de 3.3V** da Pico.

### 📈 Resultado Esperado
* **Proteção Total:** A comunicação I2C trafega rigidamente no nível seguro de 3.3V nativo do RP2040.
* **Imunidade a Ruídos:** Os resistores físicos de 4.7kΩ garantem um barramento robusto que impede o travamento da tela ou caracteres corrompidos ("tela rachada") causados pelo ruído do chaveamento do Triac e do PWM da fan.
* **Contraste:** Após ligar tudo, ajuste o trimpot azul traseiro do módulo até obter a melhor nitidez das letras.
**Método Manual (Primeira Gravação):**
Se a placa estiver zerada ou se você colocou a Pico em modo BOOTSEL segurando o botão físico da placa ao conectar no PC, use o comando direto sem o reset via serial:
```bash
make flash1
```
