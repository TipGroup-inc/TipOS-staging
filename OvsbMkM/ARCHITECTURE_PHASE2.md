# Arquitetura Técnica - Fase 2 do OvsbMkM

## Status
- Implementado: carregamento de binários Mach-O embutidos e suporte inicial para executar um binário macOS.
- Foco: `bash_bin.c`, `mach_o.c`, `kernel.c`, `Makefile`.

## Objetivo
A Fase 2 adiciona suporte para carregar um binário macOS real (`/bin/bash`) diretamente na memória do kernel e executar sua entrypoint.

## Artefatos principais
- `src/kernel/bash_bin.c` — binário Mach-O do macOS embutido como array C.
- `src/kernel/ls_bin.c` — binário Mach-O do macOS para testes adicionais.
- `src/kernel/kernel.c` — fluxo principal do kernel que chama `mach_o_load`.
- `src/kernel/mach_o.c` — loader Mach-O 64-bit básico.
- `src/kernel/mach_o.h` — estruturas de cabeçalho Mach-O e constantes.
- `Makefile` — inclui `bash_bin.c` e `ls_bin.c` no build.

## Fluxo de execução
1. `kmain()` inicializa subsistemas básicos.
2. O kernel imprime mensagem de startup na VGA.
3. `mach_o_load((void*)bash_bin, bash_bin_len)` analisa o binário embutido.
4. O loader copia segmentos do arquivo para memória em `slide = 0x2000000`.
5. O loader resolve entrypoint via `LC_MAIN` ou `LC_UNIXTHREAD`.
6. O kernel chama o retorno do loader como função `bash()`.

## Como o Mach-O é carregado
- O loader trata apenas imagens `MH_MAGIC_64` e `MH_CIGAM_64`.
- Para cada `LC_SEGMENT_64`:
  - `fileoff` e `filesize` dizem de onde copiar no arquivo.
  - `vmaddr` define o endereço virtual destino.
  - O destino é ajustado por `file_base` e um `slide` fixo.
  - Se `vmsize > filesize`, o restante é zero-inicializado.

## Ponto de entrada
- `LC_MAIN`: usa `entryoff` somado ao `slide`.
- `LC_UNIXTHREAD`: lê o endereço original de entrada a partir do state blob e converte para o slide.

## Detalhes de implementação
- `bash_bin.c` e `ls_bin.c` usam `unsigned int []` porque o conteúdo foi gerado como valores hexadecimais de 32 bits.
- `bash_bin_len` e `ls_bin_len` são definidos automaticamente como `sizeof(array)`.
- O kernel chama `bash()` diretamente; não há ainda criação de ambiente `argc/argv` ou pilha limpa.

## Limitações atuais
- Sem real relocação de símbolos ou fixups Mach-O.
- Sem suporte a `dyld` ou bibliotecas compartilhadas.
- Sem configuração de pilha de usuário específica para o processo.
- Sem VFS; binários macOS são lidos apenas de arrays embutidas.
- O loader assume `LC_SEGMENT_64` contínuo e base fixa.

## Resultado desejado
- Binário Mach-O carregado na memória corretamente.
- Entrypoint válido encontrado e chamado.
- Saída `Bash carregado! Executando...` no terminal do kernel.

## Próximos passos dessa fase
- Extrair e aplicar relocations de Mach-O.
- Suportar `LC_LOAD_DYLIB` e bibliotecas dinamicamente ligadas.
- Criar pilha de usuário e inicializar `argv`/`envp`.
- Mapear memória de maneira segura, em vez de usar apenas um slide fixo.
