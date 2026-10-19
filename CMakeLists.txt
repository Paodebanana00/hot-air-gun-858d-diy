cmake_minimum_required(VERSION 3.13)

# Inclui o arquivo de inicialização direto da pasta do seu pico-sdk
include(~/Downloads/projetos-soprador-tecado/pico/pico-sdk-2.3.1/pico_sdk_init.cmake)

project(teste_transistor C CXX ASM)

# Inicializa o SDK
pico_sdk_init()


# Cria o executável usando o seu arquivo main.c
add_executable(teste_transistor main.c)

# Vincula as bibliotecas padrão
target_link_libraries(teste_transistor pico_stdlib hardware_pwm)

# Gera o arquivo .uf2 que você precisa arrastar para a Pico
pico_add_extra_outputs(teste_transistor)
