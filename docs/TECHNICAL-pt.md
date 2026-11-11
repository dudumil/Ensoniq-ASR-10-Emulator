# Emulador Ensoniq ASR-10

Emulador do Ensoniq ASR-10 (teclado), ao nível do hardware. Corre a ROM original v1.5 e arranca o
sistema operativo a partir da imagem de disquete (testado com o OS 3.53). Nada do OS é simulado
por alto: o código 68000 original corre sobre CPU, periféricos e chips de som emulados.

## Estado

**Funciona (confirmado em testes):**
- Arranque completo: ROM v1.5, depois o OS 3.53 carregado da disquete ("LOADING SYSTEM").
- Carregar bancos e instrumentos: o `TUTORIAL BNK` carrega os 6 instrumentos e tocam.
- Afinação correta: o A4 dá 441 Hz.
- Gravar no disco (testado com SAVE BANK, incluindo "DELETE OLD VERSION?") com escrita de volta para a imagem.
- Ejetar e inserir disquetes com a máquina ligada. "DISK DRIVE NOT READY" e a releitura do
  diretório comportam-se como no teste do manual de serviço (secção 6 g/h).
- Painel completo:
  - mostrador VFD como o original: 22 carateres de 15 segmentos inclinados (14 + ponto), com o
    segmento de sublinhado do campo ativo, e os 29 indicadores (LOAD, INST, … BEAT, CLOCK) na
    metade de cima, incluindo os que piscam;
  - LEDs vermelho/amarelo dos 8 Instrument•Track;
  - os 38 botões;
  - teclado de 61 teclas com velocidade (o aftertouch polifónico existe no protocolo e na CLI, ainda não na janela).
- Pitch wheel, mod wheel, slider de volume, pedal CV e **data entry slider**, todos pelo A/D do
  OTTO e multiplexer em PB2-0. A página escondida `COMMAND → ENV1 → EXAMINE ANALOG INPUTS` mostra
  os valores que o manual de serviço indica: PITCHWHL 64, PEDAL 127, REFRENCE ≥190.
- Som:
  - 32 vozes ES5506 (OTTO) a ~29,76 kHz, reamostrado para a placa de som. Os efeitos "44KHZ"
    reduzem o OTTO a 24 vozes e o OS muda o relógio para 16,9344 MHz (44,1 kHz exatos); o emulador
    escolhe o relógio pelo número de vozes ativas, por isso estes efeitos já não baixam o tom;
  - efeitos pelo ES5510 (ESP): o OS descarrega o algoritmo e o DSP corre-o amostra a amostra.
    Os 50 efeitos da ROM (ROM-01 HALL REVERB … ROM-50 VAN DER POL) e o efeito do banco carregam
    sem erros e produzem som estável, sem auto-oscilação (teste automático com bateria em todos);
  - BUS1/2/3 entram no ESP e os AUX1-3 são misturados na saída.
- **Sequenciador.** Tocar, parar, gravar faixas (RECORD+PLAY, a gravação começa na primeira tecla;
  STOP → "xxx BARS - KEEP TRACK?" → ENTER) e andamento certo: a 100 BPM cada tempo dura 0,600 s.
  O relógio vem do timer da DUART (cristal de 4 MHz).
- MIDI In/Out pela DUART 2681 a 31250 baud (no front-end gráfico através de RtMidi).
- **Sampling.** A entrada de áudio pode vir do microfone ou da entrada de linha do computador, ou
  de um ficheiro WAV. O fluxo completo do manual funciona:
  - SAMPLE → Instrument•Track → página de nível com barras e estrela de threshold;
  - ENTER → "WAITING…", depois "RECORDING xxx SEC LEFT" quando o sinal passa o threshold;
  - CANCEL → "PLAY ROOT KEY OR ENTER".

  A amostra fica tocável no teclado. Testado com um seno de 440 Hz, que volta a tocar a 441 Hz,
  dentro da resolução da medição.
- **SCSI (opção SP-3), estilo ZuluSCSI.** Imagens de disco e de CD-ROM aparecem como drives SCSI.
  Testado com o OS 3.53:
  - FORMAT SCSI DRIVE, COPY OS TO SCSI DRIVE e criação das pastas por omissão;
  - arranque do OS a partir do disco SCSI ou de um CD-ROM, sem disquete;
  - gravar e carregar instrumentos (as leituras grandes vão por DMA);
  - CD-ROM só de leitura: o OS pede blocos de 512 bytes e gravar dá "DISK TYPE ERROR".

- **Audio Tracks (RAMTracks).** Botões A/B com os LEDs Selected e Source Monitor, monitorização da
  entrada, gravação com RECORD+PLAY, audição com "KEEP = OLD NEW" e reprodução com a sequência.
  Testado com o ATRK TUT SNG da disquete do OS: um seno de 440 Hz gravado na pista A (e na B)
  volta a tocar com a sequência.
- **Audio Tracks no disco (DiskTracks).** Com `CONFIGURE AUDIO TRACKS` (ATRK PLAY/REC = SCSI,
  REC SCSI DRIVE, REC FILE) a gravação vai para o disco SCSI, "KEEP = OLD NEW" guarda-a e a
  reprodução lê do disco enquanto a sequência toca. O buffer em RAM é gerido pelo próprio OS.
  Testado com uma imagem de 100 MB formatada pelo ASR-10 e uma voz mono a 29,762 kHz e a
  44,1 kHz: ~22 s gravados na pista A voltam a tocar à velocidade certa, com correlação de
  forma de onda de 0,95–0,97 com o original.

**Ainda não emulado / limitações:**
- SCSI: as transferências são instantâneas e não há modo target (o ASR-10 como escravo SCSI).
  Os comandos SCSI implementados são os que o OS usa: TEST UNIT READY, REQUEST SENSE, INQUIRY,
  READ CAPACITY, READ/WRITE(6/10), MODE SELECT/SENSE(6), FORMAT UNIT, START STOP, PREVENT,
  VERIFY e READ TOC.
- Output Expander: os AUX1-3 vão para a saída principal em vez de saídas separadas.
- Efeitos: o ESP segue a datasheet, mas o som ainda não foi comparado com um ASR-10 real. Os
  efeitos foram verificados por medição (estabilidade, nível, cauda de reverb), não de ouvido contra
  o original. A datasheet contradiz-se sobre o significado do bit C depois de uma subtração
  (carry ou borrow); nenhum efeito da ROM testa esse bit.
- Protocolo do painel: os comandos $67, $71, $7E e $D0+x continuam sem significado conhecido.
  O texto, os indicadores e os LEDs estão corretos.
- Indicadores do mostrador: 24 dos 29 foram confirmados a ver que índice o OS acende em cada
  página. STEP, REP e MACRO foram deduzidos da disposição em grelha (as duas metades estão ligadas
  da mesma maneira). Na página Load/Effects o OS acende SYSTEM, ENV e o índice 8 da metade direita,
  que não tem legenda; falta confirmar numa foto do aparelho real.
- Só imagens de disquete em bruto (setores de 512 bytes por ordem), HD de 1,6 MB ou DD de 800 KB.
  Formatos .EDE/.EDA/.GKH precisam de ser convertidos primeiro (p.ex. com EpsDisk ou Awave).

## Nota sobre os chips

O ASR-10 não usa o OTIS (ES5505) nem a CPU 68000 "simples" do EPS-16+:

- **CPU:** Motorola **MC68302** (núcleo 68000 + IMP: controlador de interrupções, chip selects,
  timers, IDMA, 3 SCC, portas paralelas).
- **Som:** Ensoniq **ES5506 "OTTO"**, 32 vozes, sucessor do OTIS.
- **Efeitos:** Ensoniq **ES5510 "ESP"**. A datasheet do ESP foi essencial (HALT, protocolo de
  acesso do host).
- **Disquete:** NEC **uPD72069** (compatível 765).
- **UART:** Signetics **2681** DUART: canal A = MIDI, canal B = ligação ao painel/teclado.

## Compilar

Precisa de um compilador C++17 e, para o front-end gráfico, do **SDL2**.

**Linux (Debian/Ubuntu):**
```sh
sudo apt install build-essential cmake libsdl2-dev libasound2-dev libpulse-dev libjack-jackd2-dev
cmake -S . -B build && cmake --build build -j
```
O ALSA, o PulseAudio e o JACK são opcionais: cada um que estiver instalado aparece como driver de
áudio nas Opções. Sem nenhum, o som vai pelo SDL.

**macOS (Apple Silicon):** com as Command Line Tools (`xcode-select --install`) e o CMake
(`brew install cmake`):
```sh
./build-mac.sh
```
O script compila a `ASR-10.app` (SDL2 ligado estaticamente: não precisa de bibliotecas
instaladas), o `asr10-cli` e o plug-in `ASR10.vst3`, assina-os "ad hoc" e instala o plug-in em
`~/Library/Audio/Plug-Ins/VST3`. Fica tudo em `dist/`. Na primeira vez descarrega o SDK VST3 e o SDL2.
As ROMs e o `os353.img` vão para dentro da app e do plug-in se estiverem em `roms/`; senão, ambos os
procuram em `~/Library/Application Support/Ensoniq/ASR10emu/roms`. A app trabalha sobre uma cópia
do disco do OS nessa pasta (a app em si nunca é alterada). Há também um workflow
(`.github/workflows/macos.yml`) que faz o mesmo num Mac da GitHub.

**Windows (MSYS2 / MinGW64):**
```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2
cmake -S . -B build -G "MinGW Makefiles" && cmake --build build -j
```

**Windows a partir de Linux (cross-compile, executável único sem DLLs):** com o
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) e o pacote `SDL2-devel-…-mingw`:
```sh
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=mingw64.cmake -DCMAKE_BUILD_TYPE=Release \
      -DASR10_STATIC_SDL=ON -DSDL2_DIR=<SDL2>/x86_64-w64-mingw32/lib/cmake/SDL2 -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build build-win -j
```
O `mingw64.cmake` (há um exemplo em `mingw64.cmake.example`) indica os compiladores e
`CMAKE_SYSTEM_NAME Windows`. O executável leva um manifest com página de código UTF-8, para os
caminhos com acentos funcionarem.

Também há um `Makefile` simples:
- `make` compila só o `asr10-cli`, sem dependências;
- `make asr10` compila o front-end e usa o `sdl2-config`.

### Plug-in VST3

```sh
cmake -S plugin -B build-vst -DCMAKE_BUILD_TYPE=Release
cmake --build build-vst --target ASR10
```
Descarrega o SDK VST3 da Steinberg (licença MIT desde a 3.8) e o SDL2. Opções: `-DVST3_SDK_ROOT=…` e
`-DSDL2_SOURCE_DIR=…` para usar cópias locais; `-DASR10_VST_GUI=OFF` compila sem editor (por exemplo
em Linux sem as bibliotecas do VSTGUI); `-DASR10_VST_TESTHOST=ON` compila o `minihost`, um anfitrião
mínimo sem janela para testes (`minihost ASR10.vst3 saida.wav segundos`). No fim da compilação corre o
validador oficial da Steinberg.

Windows: com o Visual Studio, `cmake -S plugin -B build-vst -A x64` e `cmake --build build-vst
--config Release`. A partir de Linux/macOS, `./build-vst-win-mingw.sh <pasta do llvm-mingw>` faz a
compilação cruzada: o script `tools/patch_vst3sdk_mingw.py` adapta uma cópia do SDK ao MinGW (sem PPL,
sem a biblioteca standalone do VSTGUI, DirectComposition desligada; o VSTGUI desenha só com Direct2D).

O plug-in usa os mesmos ficheiros que a app: a app grava a sua pasta em `asr10.cfg` (`app_dir=`) cada
vez que arranca, e o plug-in procura as ROMs e o `os353.img` na pasta `roms` da app, depois na pasta do
utilizador (`%APPDATA%\Ensoniq\ASR10emu\roms` no Windows, `~/Library/Application Support/Ensoniq/ASR10emu/roms`
no macOS, `~/.local/share/Ensoniq/ASR10emu/roms` no Linux) e por fim dentro do bundle. A disquete do OS é
o mesmo ficheiro que a app usa. Basta abrir a app uma vez antes de usar o plug-in.

O plug-in é um instrumento ("Instrument|Sampler"):
- **MIDI In**: notas, poly pressure, SysEx, e todos os controladores, pitch bend, aftertouch e program
  change dos 16 canais (por parâmetros escondidos ligados pelo `IMidiMapping`);
- **MIDI Out**: o que o ASR envia (o sequenciador pode tocar outros instrumentos);
- **saídas**: Main e AUX 1/2/3 (o Output Expander). Uma saída AUX ativada no anfitrião sai da mistura
  principal, como no rack com o OEX-6;
- **entrada estéreo** (side chain) para samplar e gravar Audio Tracks;
- **parâmetro**: Volume (o slider do painel);
- **editor**: só o painel do rack (desenhado com o mesmo código da app, a 2× para ecrãs Retina), em
  inglês, sem botões extra, como no aparelho: arrastar uma imagem `.img` ou instrumentos `.efa`/`.efe`
  para o painel mete-os no drive (ou no SCSI, se a imagem tiver mais de 2 MB); o botão de ejetar do
  drive ejeta; o botão Power mantido ~1 s faz reset; clique direito numa seta Up/Down = as duas setas
  juntas. As mensagens (disco inserido, erros) aparecem uns segundos em baixo do painel;
- **definições**: o plug-in não tem opções próprias. Usa as da app (`asr10.cfg` na pasta do
  utilizador): para mudar, por exemplo, a pasta das drives SCSI, abre a app, muda nas Options e o
  plug-in segue (lê-as ao abrir e volta a lê-las quando o ficheiro muda, com o editor aberto);
- **projeto**: guarda o volume e os caminhos da disquete e dos discos SCSI. Ao abrir o
  projeto o ASR arranca de novo com esses discos (o conteúdo da RAM não é guardado: carrega-se como
  num ASR real).

Várias instâncias no mesmo projeto funcionam: o núcleo 68000 (Musashi) tem estado global, por isso as
instâncias partilham-no à vez, trocando o contexto do CPU sob um lock.

## Ficheiros necessários

Coloca numa pasta `roms/`, ao lado do executável ou na pasta de trabalho:

- `asr-648c-lo-1.5b.bin` e `asr-65e0-hi-1.5b.bin`: as duas EPROMs v1.5 (também aceita
  `asr_lo.bin` / `asr_hi.bin`);
- a imagem do disco do OS, p.ex. `os353.img`.

## Utilização

```sh
./asr10                           # sem argumentos usa roms/os353.img (ou a primeira .img de roms/)
./asr10 roms/os353.img            # arranca com a disquete do OS no drive
./asr10 --ram 2                   # RAM de base (por omissão: 16 MB, a expansão máxima)
./asr10 --list-midi               # lista portas MIDI
./asr10 --midi-in 1 --midi-out 0  # escolhe portas pelo número (normalmente usa-se F10)
./asr10 --readonly ...            # nunca grava alterações na imagem
./asr10 --list-audio              # lista drivers e dispositivos de áudio
./asr10 --audio-driver asio       # escolhe o driver (wasapi, ds, asio, alsa, pulse, jack, core, sdl)
./asr10 --audio-in ...            # usa a entrada de áudio predefinida (Ctrl+I liga/desliga)
./asr10 --input-wav loop.wav ...  # Ctrl+P toca o WAV para a entrada do sampler
```

### Opções de áudio e MIDI (F10)

O botão **OPCOES** (canto inferior direito), F10 ou Ctrl+O abrem a página de opções. Escolhe-se com
as setas ou com o rato (clicar em `<` / `>`, ou a roda) e **APLICAR** (Enter) põe as escolhas a
funcionar e grava-as.

| Opção | O que faz |
|---|---|
| Driver de áudio | Windows: WASAPI, DirectSound, ASIO e SDL. macOS: CoreAudio e SDL. Linux: ALSA, PulseAudio, JACK (os que foram compilados) e SDL. |
| Saída de áudio | dispositivo de saída |
| Frequência / Buffer | taxa de amostragem e tamanho do buffer (latência) |
| Main (saídas) | par de saídas do Main L/R (1-2, 3-4, …) |
| AUX 1/2/3 (saídas) | par de saídas de cada bus AUX (o Output Expander OEX-6sr), ou "misturado no Main" como num ASR-10 sem expander |
| Entrada de áudio | dispositivo para o sampler (com ASIO tem de ser o mesmo da saída) |
| Entrada L / R ← canal | que entradas da placa vão para o canal esquerdo e direito do sampler |
| MIDI In / MIDI Out | portas MIDI: teclados ou controladores para tocar o ASR-10, e saída para outros sintetizadores (notas do sequenciador, MIDI OUT do ASR-10) |

- Um instrumento vai para um AUX em Edit › Amp, no parâmetro `OUT=` (BUS1-3, AUX1-3).
- O emulador corre numa thread própria, alguns milissegundos à frente da placa de som. A latência
  total é mais ou menos o buffer mais 6 ms. A página mostra quantas vezes o áudio falhou
  (underruns); se houver muitas, aumenta o buffer.
- Se o dispositivo guardado não abrir (por exemplo, a placa foi desligada), o som passa para o SDL
  e a página de opções mostra o erro.
- As escolhas ficam em `asr10.cfg` na pasta de preferências do utilizador: `%APPDATA%\Ensoniq\ASR10emu`
  no Windows, `~/.local/share/Ensoniq/ASR10emu` no Linux e `~/Library/Application Support/Ensoniq/ASR10emu`
  no macOS. É texto simples e pode ser editado à mão.

### SCSI (como num ZuluSCSI)

Põe as imagens numa pasta com os nomes que o ZuluSCSI usa e escolhe-a nas opções (F10/Ctrl+O →
**SCSI drives → Drive images folder → Choose folder...**), ou indica-a com `--scsi-dir pasta`. A escolha
fica em `asr10.cfg` (`scsi_dir=`) e o plug-in VST3 usa a mesma. Se não houver pasta escolhida, o
emulador usa uma pasta `scsi` na pasta de trabalho, na pasta das definições ou ao lado do executável.
Mudar de pasta liga logo as drives novas; reinicia o ASR (Power ~1 s) para o OS as procurar.

**Criar um disco novo:** nas mesmas opções, escolhe **New drive size** (de 50 MB a 2 GB) e carrega em
**New drive → Create HDn.img**. O emulador cria na pasta (ou em `…/ASR10emu/scsi`, se ainda não houver
nenhuma) uma imagem com o primeiro ID SCSI livre, já formatada como o FORMAT SCSI DRIVE do ASR-10 a
deixa (bloco de identificação, diretório vazio, FAT), por isso não é preciso formatá-la. O ficheiro é
criado "esparso": só ocupa espaço no disco à medida que é escrito (em NTFS ocupa o tamanho todo).

| Nome do ficheiro | Dispositivo |
|---|---|
| `HD<id>[lun][_<bloco>].<ext>` (p.ex. `HD0.img`, `HD10_512.hda`) | disco rígido, gravável |
| `CD<id>….iso` (p.ex. `CD5.iso`) | CD-ROM, só de leitura (blocos de 2048; o OS pede 512) |
| `RE<id>…` / `MO<id>…` | disco amovível / magneto-ótico, gravável |

- O ID 3 é o do próprio ASR-10, por isso imagens com ID 3 são ignoradas.
- Também se pode ligar uma imagem a um ID com `--scsi ID:imagem` (ou `--scsi ID:imagem:cd`).
- Arrastar para a janela um ficheiro com um nome destes liga-o ao barramento SCSI. Um ficheiro com
  outro nome mas maior que 2 MB (p.ex. `DISK000.iso`) também vai para o SCSI, como disco rígido no
  primeiro ID livre; o mesmo se for dado na linha de comandos. Os restantes vão para a disquete.
- O OS procura dispositivos SCSI no arranque. Para usar um disco novo:
  1. Faz `COMMAND → SYSTEM•MIDI → FORMAT SCSI DRIVE`.
  2. Depois `CHANGE STORAGE DEVICE`.
  3. Opcionalmente, copia o OS para o disco: a seguir o ASR-10 arranca de lá sem disquete.
- Uma imagem nova faz-se com, por exemplo, `truncate -s 500M HD0.img` (Linux/macOS) ou
  `fsutil file createnew HD0.img 524288000` (Windows).
- `--readonly` também abre os discos SCSI só para leitura. As escritas vão diretamente para o
  ficheiro, tal como num ZuluSCSI.

**Fazer um sample:**
1. Carrega em **SAMPLE** e num botão **Instrument•Track** vazio. Aparece a página de nível, com
   barras e a estrela do threshold.
2. Escolhe a entrada nas Opções (F10). Ctrl+I liga e desliga a entrada; Ctrl+P toca o WAV dado em
   `--input-wav`.
3. **ENTER** arma a gravação ("WAITING…"); começa quando o sinal passa o threshold.
4. **CANCEL** pára. Toca a tecla de raiz, ou ENTER, e o sample fica tocável.

Depois de "LOADING SYSTEM" aparece `FILE 1 TUTORIAL BNK`. Carrega em **ENTER/YES** para carregar
o banco e depois num botão **Instrument/Track** para tocar.

**Janela.** Em cima está a frente do ASR-10 rack, com a disposição das fotos de uma unidade
real: indicadores de entrada (Peak/Signal), Power, Phones, slider de Volume, os 12 botões de
parâmetros, Audio Tracks A/B, os botões de modo, o mostrador, os 8 botões Instrument/Track com
o LED bicolor (vermelho = carregado, amarelo = selecionado), transporte, cursores, Cancel/Enter,
slider de Data Entry e o drive de disquetes vertical. O logótipo e o nome do modelo não são
desenhados. O rack não tem teclado: por baixo há uma faixa com pitch/mod wheel e um teclado
de 61 teclas para tocar com o rato. Os dois botões PATCH SELECT dessa faixa atuam nos pinos PB10/PB11
do 68302, como os botões do ASR-10 de teclado e o jack Patch Select do rack: clique = premido enquanto
o rato está em baixo, clique direito = fica preso/solto (para ter os dois ao mesmo tempo, `**`), teclas
`[` e `]`. Na página Edit/Instrument PATCH= o indicador mostra `OO`, `*O`, `O*` ou `**`.

**Audio Tracks:** os botões A e B (ou F11/F12) selecionam a pista; carregar outra vez na pista
selecionada liga/desliga a monitorização da entrada (LED vermelho Source Monitor). Para gravar:
seleciona a sequência, liga a entrada (Ctrl+I), clica em RECORD (fica preso) e em PLAY. No fim aparece
"KEEP = OLD NEW": ENTER guarda a gravação nova. Para gravar no disco (DiskTracks), liga um disco
SCSI formatado pelo ASR-10 e, em `COMMAND → SYSTEM•MIDI → CONFIGURE AUDIO TRACKS`, põe
ATRK PLAY/REC = SCSI, escolhe o REC SCSI DRIVE e o tamanho do REC FILE.

**Rato:**
- clica nos botões e no teclado (mais abaixo na tecla = mais forte);
- arrasta os sliders de Volume e Data Entry e os wheels;
- a roda do rato faz Up/Down;
- mantém o **Power** carregado ~1 s para reiniciar (como desligar e ligar);
- o **RECORD** fica preso: um clique carrega-o, outro clique solta-o (para RECORD+PLAY e outras
  combinações sem ter de segurar o botão);
- o botão **TECLADO** (ou F9) esconde/mostra a faixa com o teclado, os wheels e o Patch Select;
  a janela fica só com o rack. A escolha fica guardada nas definições;
- o botão do drive ejeta a disquete (gravando antes, se foi alterada); o LED do drive acende com
  o motor;
- arrasta um ficheiro `.img` para a janela para o inserir no drive;
- arrasta um ou mais instrumentos `.efa`/`.efe` para a janela: vão para uma disquete temporária
  (alta densidade se for preciso) que entra no drive; depois é só LOAD como de qualquer disquete;
- **clique direito numa das setas Up/Down** = as duas setas ao mesmo tempo: o valor salta para o
  meio da escala (0 em -99…+99, 50 em 0…99), como no aparelho. O mesmo com as duas setas do
  teclado do PC carregadas juntas.

**Teclado do computador:**

| Tecla | Função |
|---|---|
| 1–8 | Instrument•Track 1–8 (o teclado do PC já não toca notas) |
| R | RECORD (mantém-no e carrega em Espaço para gravar) |
| Setas | Up / Down / Left / Right (Up + Down juntas = meio da escala) |
| Enter | ENTER•YES |
| Backspace / Esc | CANCEL•NO |
| F1–F8 | Instrument•Track 1–8 |
| teclado numérico 0–9 | botões de página como impressos: 1 Env 1 … 4 Pitch … 9 Layer, 0 Track |
| Espaço | PLAY |
| F11 / F12 | Audio Track A / B |
| F9 | esconder/mostrar o teclado |
| Ctrl+E | ejetar disquete (grava antes, se foi alterada) |
| Ctrl+S | gravar a disquete na imagem |
| Ctrl+F | ESP ligado/desligado (som seco) |
| Ctrl+R | reset |
| Ctrl+I | liga/desliga a entrada de áudio escolhida nas Opções |
| F10 / Ctrl+O | página de opções (áudio e MIDI) |
| Ctrl+P | toca o ficheiro de `--input-wav` para a entrada do sampler |

Tal como num drive real, as escritas do OS vão para a imagem. Ficam gravadas no ficheiro ao
ejetar, com Ctrl+S ou ao sair, exceto com `--readonly`. Trabalha sobre cópias das tuas imagens.

### Ferramenta de linha de comandos (`asr10-cli`)

Corre sem janela, à velocidade máxima (cerca de 10× tempo real). Serve para testes, para
reverse-engineering e para renderizar áudio. Cada mudança do mostrador é impressa com o tempo,
o texto e, entre `< >`, os indicadores acesos (`*` = a piscar), por exemplo
`<LOAD* INST BANK | STOP>`.

```sh
./asr10-cli --disk roms/os353.img --seconds 24 \
    --button 12.0:35:1 --button 12.2:35:0  \
    --button 19.5:2:1  --button 19.6:2:0   \
    --key 20.0:60:100:1.5 --wav nota.wav
```

O exemplo carrega em ENTER para carregar o banco, seleciona o instrumento 1 e toca o C4.

| Opção | O que faz |
|---|---|
| `--button t:idx:1/0` | botão (índice do painel, ver tabela abaixo; 40/41 = Audio Track A/B) |
| `--key t:nota:vel:dur` | tecla do teclado |
| `--note t:nota:vel:dur` | nota por MIDI |
| `--analog-at t:canal:valor` | controlos analógicos |
| `--input-tone t:freq:amp:dur[:L/R]` | seno na entrada de áudio |
| `--input-wav t:ficheiro.wav` | WAV de 16 bits na entrada de áudio |
| `--wav f.wav` | grava a saída principal |
| `--aux-wav f.wav` | separa os AUX1-3 do Main (como com o Output Expander) e grava-os somados neste ficheiro |
| `--panel-raw t:b1:b2` | mensagem crua painel→CPU (hex) |
| `--hexdump addr n` | despeja n bytes da memória no fim |
| `--scsi-dir pasta` / `--scsi id:img[:cd]` | dispositivos SCSI |
| `--scsi-log` | registo dos comandos SCSI |
| `--no-scsi` | sem a opção SP-3 |
| `--insert t:img` / `--eject t` | trocar disquete |
| `--write-back` | grava a imagem no fim |
| `--trace-io` | registo de acessos de I/O |
| `--panel-log` | registo do protocolo do painel |
| `--fdc-log` | registo da controladora de disquete |
| `--break PC` | breakpoint |
| `--trace-from PC n` | traça n instruções a partir de PC |
| `--dis addr n` | disassembler |
| `--dump-ram f` | despeja a RAM para o ficheiro f |
| `--esp-list` | lista o programa carregado no ESP |
| `--otto-dump` | estado das vozes do OTTO |

Variáveis de ambiente para depurar o ESP:

| Variável | Efeito |
|---|---|
| `ESP_TRACE=n,r1,r2,…` | de n em n frames, mostra os GPRs r1, r2… (em hex) e o acumulador |
| `ESP_ITRACE=f` | traça cada operação da ALU e do multiplicador nos frames f e f+1 |
| `ESP_HOSTLOG=1` | regista as escritas do 68000 em GPRs e instruções do ESP |

## Mapa de hardware (reverse-engineered)

**Memória e chip selects**

| Endereço | Dispositivo | Notas |
|---|---|---|
| `$000000` | RAM | 2 MB de base, expansões até 16 MB. A RAM é também a memória de amostras do OTTO (banco 1 = `bank^1`). |
| `$F80000-$FBFFFF` | ROM (CS0) | 2 × 27C010 entrelaçadas (`hi` = byte par). |
| `$FC2001 + 2·n` | OTTO (CS2) | registos ES5506, n = 0..63 |
| `$FC3001 + 2·n` | ESP (CS2) | registos de host do ES5510, n = 0..$FF |
| `$FC4001/3` | uPD72069 (CS3) | MSR/comandos auxiliares, dados |
| `$FC4801-1F` | DUART 2681 | registo = A4-A1 |
| `$FC5001/3` | WD33C93 SCSI | `$FC5001`: escrita = registo de endereço, leitura = auxiliary status. `$FC5003`: registos, com auto-incremento. |
| `$FC5401` | latch do SCSI | o OS escreve `$FE` antes de cada DMA |
| `$FC5801/3` | porta de DMA | partilhada pela disquete e pelo WD33C93; usada pelo IDMA do 68302 (pedido externo) |
| `$FC6000` | 68302 | BAR = `$0FC6` (dual-port RAM e registos internos) |
| `$FF7F00-$FF7FFF` | CS1 (só escrita) | **mapa de páginas do OTTO**: 32 × 4 words. A escrita vai também para a RAM no mesmo endereço; o OS lê de lá o mapa de volta. |

**Interrupções** (GIMR = `$8040`, vetores fornecidos pelo IMP)

| Fonte | Ligação |
|---|---|
| IRQ1 | INT do FDC e INTRQ do WD33C93, em wired-OR. O handler do OS vê qual é o dispositivo ativo e salta para a continuação em `$402`. |
| IRQ6 | DUART |
| PB9 (flanco descendente) | IRQ do OTTO |
| SCC1/SCC2 | A/D esquerdo/direito do sampling |
| TMR2 | tick de ~1 kHz |

**Portas do 68302**

| Pino | Função |
|---|---|
| PA4 | HALT do ESP (0 = halt, invertido para o pino HALT ativo-alto) |
| PA5/PA6 | LEDs "Selected" (amarelos) das Audio Tracks A/B, ativos a 0. Piscam (o OS troca o bit no tick do TMR2) durante o sampling stereo. |
| PA11/PA12 | LEDs "Source Monitor" (vermelhos) das Audio Tracks A/B, ativos a 0 |
| PB3 | LRCLK. Se faltar, dá o erro 009 "No LRCLK input to 68302". |
| PB2-0 + PB4 | seleção do multiplexer analógico |
| PB5/PB6 | botões Audio Track A/B (ativos a 0). Não passam pelo micro do painel: o OS lê-os no tick do TMR2 e gera os códigos de função 8 e 9. |
| PB10/11 | Patch Select esquerdo/direito (botões do ASR-10 de teclado e jack Patch Select de trás), ativos a 0. Uma descida gera a interrupção PB10/PB11; o handler arma o debounce e o OS lê os pinos no tick do TMR2 (estado em $C42: bit 6 = esquerdo, bit 5 = direito; valor do controlador 0/32/64/127). |

**SCC em modo síncrono (A/D).** O OS lê o "Rx internal data pointer" (+$88) da parameter RAM de
cada SCC para saber onde começa a gravação de uma Audio Track, e encurta o "Rx internal byte
count" (+$8C) para alinhar os buffers do SCC1 (esquerdo) e do SCC2 (direito). O emulador mantém os
dois campos atualizados byte a byte e fecha o buffer quando a contagem chega a 0. Sem isto a
gravação nunca começava e o OS ficava preso ao parar.

**Ligação ao painel (DUART canal B).** Ao começar a gravar (RECORD+PLAY) o OS faz reset ao
recetor e volta a configurá-lo. Os bytes que o painel envia nesse intervalo (os "soltar" dos
botões) ficam à espera em vez de se perderem, e nunca entram num FIFO cheio; senão o OS ficava a
pensar que o RECORD continuava carregado.

**Entradas da DUART**

| Pino | Função |
|---|---|
| IP0 | READY do drive |
| IP1 | disk change. O OS testa o delta em IPCR bit 5 depois de cada SENSE DRIVE STATUS. |
| IP2 | disquete HD |

**ESP:** o OS escreve em `$1F` (Halt Enable) com HALT ativo e descarrega o programa. Enquanto
está parado, o registo PC conta os ciclos de refresh. A seguir verifica tudo e põe o ESP a correr.
Pormenores do ES5510 que os efeitos exigem (todos da datasheet):

- O teste de *skip* de uma instrução usa o CCR escrito pela instrução anterior. Os programas fazem
  `CMP` e logo a seguir uma instrução condicional.
- Flags:
  - ABS não mexe em Z;
  - ASL2/ASL8 não mexem em C;
  - LS15 põe em C o bit 8 do operando;
  - instruções marcadas como *skippable* nunca alteram o CCR.
- Depois de END a execução recomeça na instrução 0.
- Com HALT, o ESP acaba a instrução END (MAC, ALU e escrita na RAM) antes de parar e esvazia o
  FIFO do DOL. Sem isto, o resto do pipeline do efeito antigo escrevia num GPR do efeito novo. No
  ROT.SPKR+REV isso apagava um offset de linha de atraso e o efeito saturava.
- O OS lê de volta os registos especiais para verificar a carga. Os bits não implementados leem 1:
  DLENGTH/ABASE/BBASE nibble baixo, SIGREG, CCR, CMR e SER3.
- As tabelas que o OS carrega na RAM do ESP (curvas de distorção e de compressão, por exemplo) usam
  o endereço DADR alinhado à esquerda, como os offsets dos GPRs: palavra = (DADR & máscara) >>
  MEMSIZ. Antes disto, os efeitos com tabelas (CMP+DIST+REV, WAH+DIST+REV, GUITAR AMP, EQ+CMPRESSOR,
  DE-ESSER, DUCKER) saíam mudos ou muito baixos no BUS1.
- Os endereços da RAM do ESP formam-se em palavras inteiras:
  - os bits dos registos de base (ABASE, BBASE, DBASE) abaixo do MEMSIZ não entram na soma, nem
    a parte fracionária dos offsets interpolados. O OS deixa lixo nesses bits (ABASE = `$xxxx0F`;
    o DBASE recarrega de DLENGTH = `$FFFFFF`). Somá-los fazia o endereço saltar uma amostra sempre
    que a fração do offset passava de ~0,94, e os efeitos que interpolam a linha de atraso (PITCH
    SHIFT, PITCH+DDL, FAST PITCHSH, chorus, flanger) estalavam;
  - o módulo da linha de atraso é (DLENGTH >> MEMSIZ) + 1 palavras. Feito sobre os valores de 24
    bits, as leituras que davam a volta ao fim da memória caíam uma palavra ao lado, uma vez por
    volta: um ruído a cada 65536 amostras (~1,5 s a 44,1 kHz).

**Mostrador.** VFD de 22 carateres de 15 segmentos (a–f, meio esquerdo/direito, duas verticais
centrais, quatro diagonais e ponto) mais um segmento de sublinhado por carácter. Os padrões de
segmentos foram tirados de fotos de um ASR-10 real. O zero é cortado; a letra O não.

- O gerador de caracteres mostra os códigos `! # % ( ) : ; [ \ ]` como os dígitos 0–9 com ponto.
  O OS usa-os, por exemplo, em "BAR=001.01" (tabela em `$F824BE` da ROM). A CLI mostra os códigos
  em bruto.
- Indicadores: `$77 n` / `$78 n` / `$79 n` acendem, apagam ou põem a piscar o indicador n da
  metade esquerda; `$7A`/`$7B`/`$7C` fazem o mesmo na metade direita. Algumas páginas fazem piscar
  pelo próprio OS, alternando `$77`/`$78`.

| n | esquerda | direita | n | esquerda | direita |
|---|---|---|---|---|---|
| 1 | MACRO | BAR | 9 | LFO | CLOCK |
| 2 | SEQ | SONG | A | PITCH | STEP |
| 3 | MIDI | REC | B | LAYER | STOP |
| 4 | SONG | SEQ | C | SYSTEM | PLAY |
| 5 | EDIT | TRACK | D | CMD | AMP |
| 6 | FILTER | REP | E | INST | ODUB |
| 7 | BANK | BEAT | F | LOAD | ENV |
| 8 | WAVE | (sem legenda) | 0 | — | — |

As legendas da janela são bitmaps gerados por `tools/make_vfd_art.py` (posições medidas numa foto
do vidro, letra FreeSans Bold Oblique) e guardados em `src/vfd_art.h`.

**DUART.** O cristal X1 é de 4 MHz. O timer (modo 6) gera a onda quadrada com período 2 × valor
inicial, e o bit "counter ready" do ISR acende uma vez por período. É este o relógio do sequenciador.

**Multiplexer analógico → POT do OTTO**

| Canal | Controlo |
|---|---|
| 0 | pitch wheel (centro calibrado no arranque) |
| 2 | mod wheel |
| 3 | slider de volume |
| 4 | pedal CV |
| 5 | data entry slider |
| 7 | referência |

**OTTO e Transwaves.** O MAME para uma voz quando START = END. O OS do ASR-10 passa por START = END
enquanto desloca a janela de loop de uma Transwave (por exemplo com a mod wheel), por isso no
emulador essas notas cortavam de vez em quando. Esse atalho foi retirado.

**Memória de amostras.** Num ASR-10 de 2 MB, o OS vê a RAM de amostras em `$600000-$7FFFFF`
(o mesmo DRAM de `$000000`, que é a RAM de sistema). O OTTO não acede diretamente à RAM: cada voz
tem 4 páginas de 1 MB, escritas pelo OS em `$FF7F00 + 8·slot`, com slot = voz − 1. O endereço
físico é `(página[(endereço do OTTO >> 20) & 3] << 20) | (endereço do OTTO & $FFFFF)`.

**Sampling.** O A/D entra em série nos SCC1 (esquerdo) e SCC2 (direito), em modo BISYNC com
relógio e sincronismo externos, uma palavra de 16 bits (MSB primeiro) por frame de áudio. Os SCC
enchem anéis de 8 × 800 bytes. Depois o OS copia cada buffer para a memória de amostras por IDMA
memória→memória (CMR `$37A1`); o bit 0 do CSR do IDMA é o DONE. A página de nível usa o comando
de painel `$73 a b`, com b = número de células acesas.

**SCSI.** O driver do OS (na ROM, `$FBAD00-$FBB5C0`) usa sempre o comando Select-and-Transfer (`$09`):

1. Escreve o CDB nos registos 3 e seguintes, o contador de bytes nos registos `$12-$14` e o ID de
   destino no registo `$15`.
2. Os dados passam por polling do bit DBR ou por DMA (registo de controlo `$80`).
3. No fim espera a interrupção `$16` (Select-and-Transfer concluído) e depois `$85` (o target
   desligou-se).
4. O byte de estado SCSI fica no registo `$0F`.

**Áudio**

| Canais do OTTO | Destino |
|---|---|
| 0, 1, 2 (BUS1, BUS2, BUS3) | SER0, SER2, SER3 do ESP |
| SER1 do ESP | DAC principal |
| 3, 4, 5 (AUX1-3) | Output Expander |

**Protocolo do painel** (DUART canal B, 31250 baud). O firmware do microcontrolador do teclado do
ASR-10 não estava disponível, por isso o protocolo foi deduzido do parser do OS (`$FFB392`).

CPU → painel. O painel responde `$FF` a cada byte; à pergunta `$FC` responde `$FC,$00`. Nos
testes, responder `$01` faz o OS tratar a unidade como rack: ignora os wheels e o PITCHWHL fica a 0.

| Byte | Comando |
|---|---|
| `$00-$15` | posição do cursor |
| `$20-$5F` | caracteres |
| `$66` | limpar |
| `$60 a` | atributo do texto seguinte (3 = sublinhado), válido só até ao `$72` |
| `$62` / `$63` | guardar a posição do cursor (início de um campo) / voltar a ela |
| `$72` | fim de campo (apaga o resto do texto antigo do campo, se o novo for mais curto) |

Com uma song a tocar, a página EDIT → SEQ é reescrita pelo contador de compassos a meio da edição
de um campo. Um byte de posição do cursor abandona o `$63` pendente, e o texto escrito fora de um
campo não herda o sublinhado do campo; sem isto o cursor saltava e deixava lixo ao andar para a
direita.
| `$65` / `$6A` | cursor para a esquerda / direita |
| `$67` / `$69` | cursor de edição (sublinhado) ligado / desligado na posição atual. É o cursor das layers na página PATCH= e das letras ao mudar um nome. |
| `$68` | marca o carácter atual com o cursor e avança. Três seguidos marcam a escolha em "KEEP = OLD NEW" (EVENT EDIT, QUANTIZE e outros comandos de faixa/sequência). |
| `$73 a b` | medidor de nível do sampling (b = células acesas) |
| `$77`/`$78`/`$79 n`, `$7A`/`$7B`/`$7C n` | indicadores do mostrador (ver acima) |
| `$74/$75/$76 n` | LED n off/on/pisca. 0-7 = vermelhos "loaded", 8-15 = amarelos "selected". |

Painel → CPU:

| Mensagem | Significado |
|---|---|
| `[$80\|idx][0]` | botão premido |
| `[idx][0]` | botão largado |
| `[$80\|k][vel]` | tecla premida, k = nota − 36 |
| `[k][vel]` | tecla largada |
| `[$40\|k][pressão]` | aftertouch polifónico |

**Índices dos botões** (tabela em `$F82484` da ROM)

| idx | botão | idx | botão | idx | botão |
|---|---|---|---|---|---|
| 0/1 | footswitches direito (sustain) / esquerdo (programável), lidos pelo micro do teclado | 2,8,14,20,4,34,28,22 | Instrument•Track 1-8 | 26 | LOAD |
| 6 | COMMAND | 5 | EDIT | 15 | INSTRUMENT |
| 21 | SEQ•SONG | 27 | SYSTEM•MIDI | 9 | EFFECTS |
| 7 | FX SELECT•BYPASS | 32 | SAMPLE | 13,18,19,24,25,30,31,36,37,12 | 1…9, 0 |
| 10/11 | Up/Down | 16/17 | Left/Right | 33 | CANCEL•NO |
| 35 | ENTER•YES | 3 | RECORD | 29 / 23 | PLAY / STOP•CONTINUE (provável) |

## Estrutura do código

| Ficheiro | Conteúdo |
|---|---|
| `src/asr10.*` | a máquina: mapa de memória, ligações entre chips, mistura de áudio |
| `src/m68302.*` | periféricos do MC68302: interrupções, portas, timers, chip selects, IDMA, SCC |
| `src/es5506.*` | OTTO |
| `src/es5510.*` | ESP |
| `src/fdc.*` | uPD72069 |
| `src/duart.*` | 2681 |
| `src/panel.*` | painel e teclado |
| `src/scsi.*` | WD33C93 e discos/CD-ROM SCSI em imagem |
| `src/sdl_main.cpp` | front-end gráfico |
| `src/rack_offscreen.*` | desenho do painel fora do ecrã (SDL software renderer), usado pelo plug-in |
| `src/mac/` | Info.plist e ícone da app de macOS |
| `plugin/` | plug-in VST3: `asr10_plugin.*` (componente), `asr10_engine.*` (máquina e partilha do 68000), `asr10_editor.*` (editor VSTGUI), `test/minihost.cpp` |
| `build-mac.sh` | compilação da app e do plug-in em macOS |
| `src/vfd.*`, `src/vfd_art.h` | desenho do mostrador VFD (segmentos e indicadores) |
| `src/rack_ui.*`, `src/panel_art.h` | frente do rack: painel, botões, LEDs, sliders, drive e legendas |
| `tools/make_panel_art.py` | gera `src/panel_art.h` (legendas do painel em FreeSans) |
| `tools/make_vfd_art.py` | gera `src/vfd_art.h` |
| `src/cli.cpp` | ferramenta sem janela |
| `musashi/` | núcleo 68000 |
| `third_party/` | RtMidi e fonte 8×8 |

## Licenças e créditos

- **Musashi**, núcleo 68000, de Karl Stenerud: ver `musashi/LICENSE-Musashi.txt`.
- **ES5506 e ES5510:** algoritmos derivados do MAME, licença BSD-3-Clause. O ES5506 é de Aaron
  Giles e o ES5510 de Christian Brunschen. Ver `LICENSES/BSD-3-Clause-MAME.txt`.
- **RtMidi** e **RtAudio**, de Gary P. Scavone: ver `third_party/RtMidi-LICENSE.txt` e
  `third_party/rtaudio/RtAudio-LICENSE.txt`.
- **ASIO:** o suporte ASIO no Windows usa os ficheiros do ASIO SDK da Steinberg
  (`third_party/rtaudio/include/asio*`). A Steinberg disponibiliza o SDK também sob GPLv3, por isso
  um executável compilado com ASIO fica sob GPLv3. Sem ASIO: `-DASR10_ASIO=OFF`. ASIO é marca
  registada da Steinberg Media Technologies GmbH.
- **font8x8**, de Daniel Hepper: domínio público.
- **VST3 SDK** e **VSTGUI**, da Steinberg Media Technologies: licença MIT (SDK 3.8). VST é marca
  registada da Steinberg Media Technologies GmbH.
- **SDL2**: licença zlib (ligado estaticamente na app de macOS e no plug-in).
- Legendas do mostrador e do painel compostas com **GNU FreeSans** (Bold Oblique no mostrador,
  regular no painel; GPLv3 com exceção para fontes). Só estão incluídas imagens já desenhadas,
  não o ficheiro da letra.
- A ROM e o sistema operativo do ASR-10 são propriedade da Ensoniq/E-mu e **não** estão incluídos.
