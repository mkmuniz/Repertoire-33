# Plano de desenvolvimento — E33 Picto Optimizer e Boss Music Swapper

Dois mods independentes para Clair Obscur: Expedition 33, ambos como **overlay in-game arrastável e minimizável**, no espírito do overlay do Mobalytics para TFT.

---

## Decisão de arquitetura: por que overlay in-game e não janela externa

O Mobalytics e o Blitz são janelas externas always-on-top porque **precisam ser**: League e TFT têm anti-cheat e não permitem injeção. O E33 é single-player, sem anti-cheat, e já tem uma plataforma de modding madura (UE4SS). Isso muda tudo a seu favor:

| | Janela externa (Electron/Tauri) | Overlay in-game (UE4SS + ImGui) |
|---|---|---|
| Dados do jogo | OCR ou parsing de save (defasado) | **Leitura direta da memória, em tempo real** |
| Fullscreen | Só funciona em borderless | Funciona em qualquer modo |
| Alt-tab | Perde foco, atrapalha | Não existe o problema |
| Instalação | Dois artefatos (mod + app) | **Um artefato só** |
| Arrastar/minimizar | Você implementa | Nativo do ImGui |

O ImGui já entrega janela arrastável, redimensionável, colapsável e com posição persistida em `imgui.ini`. Você não escreve nada disso.

**Consequência importante para o otimizador:** como você lê o estado do jogo ao vivo, some a necessidade de parsing de save e de OCR. O usuário abre o overlay e os pictos dele já estão lá. Isso é melhor do que o Mobalytics consegue fazer.

### Toolchain única para os dois

Ambos viram mods C++ do UE4SS, compilados com xmake. Você aprende uma stack e entrega dois projetos. O custo é sair do TypeScript; o ganho é não ter processo sidecar, não ter IPC e não pedir ao usuário que rode dois programas.

> **Verifique antes de começar:** confirme na documentação da versão de UE4SS que você for usar como os mods C++ registram uma janela ImGui própria, e se ela renderiza como overlay sobre o jogo ou em janela separada (isso é configurável e mudou entre versões). Esse é o pré-requisito de tudo.

### Prototipagem em Lua

Use Lua para descobrir coisas (qual classe dispara o combate, onde estão os pictos equipados) porque recarrega sem fechar o jogo. Depois porte para C++. Não tente fazer o produto final em Lua: gerenciamento de estado e UI complexa ficam ruins.

---

## Estrutura do repositório

Dois repositórios separados, com uma biblioteca compartilhada por submódulo ou copiada:

```
e33-modkit/                     # compartilhado pelos dois mods
├── include/e33/
│   ├── OverlayWindow.hpp       # chrome do overlay: arrastar, minimizar, hotkey
│   ├── Config.hpp              # JSON com hot reload
│   ├── GameData.hpp            # loader dos JSONs extraídos
│   └── Log.hpp
└── xmake.lua
```

```
e33-picto-optimizer/
├── src/
│   ├── dllmain.cpp
│   ├── Game/
│   │   ├── ReadParty.cpp       # lê party, pictos equipados, armas
│   │   └── ReadInventory.cpp   # lê pictos e luminas possuídos
│   ├── Calc/
│   │   ├── Formula.cpp         # a fórmula de dano, isolada, sem I/O
│   │   ├── Buffs.cpp
│   │   └── Calculate.cpp       # API pública
│   ├── Optimizer/
│   │   ├── Prune.cpp           # descarta pictos dominados
│   │   ├── Search.cpp          # branch and bound + beam search
│   │   └── Job.cpp             # roda em thread separada
│   └── UI/
│       ├── OverlayRoot.cpp     # janela principal
│       ├── PanelBuild.cpp      # build atual e dano
│       ├── PanelOptimize.cpp   # resultados da busca
│       └── PanelCompare.cpp    # build atual vs sugerida
├── data/1.5.0/*.json           # tabelas extraídas
├── tools/extract.ts            # Node, gera os JSONs a partir do dump do FModel
└── xmake.lua
```

```
e33-boss-music/
├── src/
│   ├── dllmain.cpp
│   ├── Hooks/
│   │   ├── CombatStart.cpp     # detecta início de combate + id do encontro
│   │   └── AudioSwap.cpp       # troca o evento de áudio disparado
│   ├── Config/
│   │   ├── Overrides.cpp       # tabela boss -> faixa, VAZIA por padrão
│   │   └── Defaults.cpp
│   └── UI/
│       ├── OverlayRoot.cpp
│       ├── PanelBosses.cpp     # lista à esquerda
│       └── PanelTracks.cpp     # lista à direita + preview
├── data/
│   ├── bosses.json             # id do encontro -> nome legível
│   └── tracks.json             # id da faixa -> nome legível
└── xmake.lua
```

**Regra de legibilidade:** `Calc/` não sabe o que é ImGui e `UI/` não sabe o que é ponteiro de objeto do Unreal. Quando um patch quebrar o mod, o estrago vai estar em `Game/` e `Hooks/`, e você conserta um diretório.

---

# Projeto A — E33 Picto Optimizer

## Milestones

### M0 — Reconhecimento (1 a 2 dias)
Protótipo em Lua que imprime no console o nome de um picto equipado.

**Pronto quando:** você consegue navegar do objeto de personagem até a lista de pictos equipados e ler um nome.

Este é o milestone que decide se o projeto é viável no formato overlay. Faça primeiro, antes de qualquer outra coisa.

---

### M1 — Overlay vazio (2 a 3 dias)
Mod C++ que abre uma janela ImGui arrastável com uma hotkey, mostrando "Hello". Inclui:

- hotkey configurável para abrir e fechar;
- captura de mouse só quando a janela está aberta (o jogo não pode ficar preso);
- posição e tamanho persistidos entre sessões;
- escala de fonte ajustável (essencial em 4K).

**Pronto quando:** você abre, arrasta, minimiza, fecha e reabre e a janela volta onde estava.

---

### M2 — Extrator de dados (3 a 4 dias)
Script Node (`tools/extract.ts`) que transforma o dump do FModel em JSON tipado, validado contra JSON Schema.

Tabelas: pictos, luminas, armas, skills, inimigos.

**Pronto quando:** `pnpm extract --version 1.5.0` gera os JSONs e falha alto se uma coluna esperada sumiu.

Coloque isso no CI. É assim que você descobre que o patch quebrou os dados antes dos usuários descobrirem.

---

### M3 — Leitura do estado ao vivo (4 a 6 dias)
O overlay mostra, em tempo real: party atual, pictos equipados por personagem, luminas ativas, arma equipada, stats resultantes.

**Pronto quando:** você troca um picto no menu do jogo e o overlay atualiza sozinho.

Este é o momento de publicar uma primeira versão. Um overlay que só mostra a build já tem público, e te dá feedback antes de você escrever a parte cara.

---

### M4 — Motor de cálculo (1 a 2 semanas)
Fórmula de dano com breakdown por multiplicador: base, buffs, break, fraqueza elemental, crítico.

**Pronto quando:** o erro médio contra as fixtures medidas fica abaixo de 2%.

O breakdown importa mais que o número final. É o que faz o usuário confiar no resultado em vez de achar que é chute.

Como validar sem jogar 40 horas:

1. Use o save com todos os bosses liberados (existe no Nexus) como save de desenvolvimento.
2. Use o save editor existente para montar builds específicas rapidamente.
3. Cada fixture é um JSON: build exata, alvo, skill, dano real observado.
4. Vinte fixtures bem escolhidas (com e sem break, com e sem fraqueza, crítico, buff empilhado) valem mais que duzentas aleatórias.
5. Um teste agregado falha se o erro médio passar do limite. Publique esse número no README.

---

### M5 — Otimizador (1 a 2 semanas)
Busca da melhor combinação de pictos e luminas dentro dos slots e do orçamento disponível.

Ordem de implementação:

1. **Poda de dominados.** Picto com mesmo custo e stats estritamente piores sai antes de enumerar. Costuma cortar uma ordem de grandeza.
2. **Branch and bound** com limite superior otimista.
3. **Beam search** para os casos que ainda explodem.

**Roda em thread separada, obrigatoriamente.** Se a busca travar o frame do jogo, o mod é inutilizável. A UI mostra progresso e um botão de cancelar.

**Pronto quando:** uma busca típica termina em menos de 3 segundos sem queda perceptível de FPS.

---

### M6 — Comparação e polimento (1 semana)
Painel lado a lado: build atual contra build sugerida, com o delta de dano por skill e a lista exata do que trocar.

Mais: modo compacto (só o número de dano, para quem quer o overlay sempre aberto), exportar build como texto para colar no Discord, e um toggle para ignorar pictos ainda não obtidos.

---

## Instalação (Picto Optimizer)

**Pré-requisito:** UE4SS instalado em `Expedition 33\Sandfall\Binaries\Win64\`.

1. Extraia a pasta `PictoOptimizer` para:
   ```
   Expedition 33\Sandfall\Binaries\Win64\ue4ss\Mods\PictoOptimizer\
   ```
2. Confirme que o arquivo `enabled.txt` existe dentro da pasta.
3. Inicie o jogo.
4. Pressione a hotkey (padrão sugerido: **F8**) para abrir o overlay.

Estrutura do pacote distribuído:

```
PictoOptimizer/
├── dlls/main.dll
├── data/1.5.0/*.json
├── config.json
└── enabled.txt
```

**Escolha da hotkey:** evite conflito com mods populares do E33. O Gramophone Everywhere usa a tecla **J**; não use essa.

---

# Projeto B — E33 Boss Music Swapper

## O princípio de design: nada muda até o usuário mandar

Os mods de música que existem hoje substituem arquivos dentro do `.pak`, então a troca é permanente e global. O autor dos mods de música de batalha documentou o motivo: a música de batalha não fica num lugar central, e sim espalhada pelos arquivos, principalmente nos mapas dos níveis, que são grandes demais para empacotar. A saída dele foi editar os metadados das faixas, o que gera uma situação de tudo ou nada e o obrigou a publicar três variantes do mesmo mod.

Seu mod resolve isso em runtime:

- `config.json` nasce **vazio**, o jogo continua 100% vanilla;
- cada override só existe se o usuário criar pelo menu;
- sem entrada para um boss, o hook não faz nada e toca o original;
- cada linha tem "voltar ao original";
- **um toggle global desativa todos os overrides de uma vez**, sem desinstalar — útil para gravar vídeo ou isolar bug;
- um download só, sem variantes.

Como a config é um JSON, quem quiser montar uma trilha alternativa inteira compartilha o arquivo, sem publicar um mod novo.

```json
{
  "enabled": true,
  "overrides": {
    "Boss_Simon": "Track_Renoir",
    "Boss_Sirene": "Track_UneVieATaimer"
  }
}
```

---

## Milestones

### M0 — Detectar o combate (2 a 4 dias)
Hook que imprime no log o identificador do encontro quando um combate começa. Nada mais.

Use o README do mod AutoParryAnim como referência: ele é um mod UE4SS em C++ para o E33 e documenta hooks nas classes de combate.

**Pronto quando:** você entra em três bosses diferentes e vê três ids diferentes no log.

---

### M1 — Catálogo (2 a 3 dias)
Mapeie ids de encontro para nomes legíveis (`bosses.json`) e ids de faixa para nomes legíveis (`tracks.json`). Cruze com `DT_jRPG_Encounters`, a mesma tabela que o randomizer sobrescreve.

**Pronto quando:** o log diz "Combate iniciado: Sirène" em vez de um hash.

---

### M2 — Troca de faixa, hardcoded (3 a 5 dias)
Substituir a faixa de um boss específico por outra faixa **nativa do jogo**, com o mapeamento fixo no código.

**Por que começar só com faixas nativas:** sem conversão de arquivo, sem Wwise, sem questão de licenciamento, e preserva a estrutura interativa da trilha. A música do E33 foi composta para suavizar nos períodos de recuperação e intensificar no clímax da luta; trocar por um loop simples perde isso e soa pior que o original.

**Pronto quando:** você entra no boss X e ouve a música do boss Y, com a dinâmica intacta.

---

### M3 — Config com fallback e hot reload (2 a 3 dias)
Tabela em JSON, vazia por padrão, recarregável sem reiniciar o jogo.

**Pronto quando:** você edita o JSON no editor, clica em recarregar e o próximo combate já usa a nova faixa.

---

### M4 — Overlay (4 a 6 dias)
Janela ImGui arrastável: lista de bosses à esquerda, lista de faixas à direita, busca por nome, botão de preview, "voltar ao original" por linha e o toggle global no topo.

**Pronto quando:** dá para configurar tudo sem abrir um editor de texto.

---

### M5 — Polimento e lançamento (3 a 5 dias)
- Log verboso opcional (sem isso você depura no escuro quando alguém disser "não funcionou no boss X");
- presets exportáveis e importáveis;
- README com screenshot, versão do jogo testada e as três linhas de instalação.

---

### M6 — Áudio externo (opcional, 1 a 2 semanas)
Permitir que o usuário use arquivos próprios, convertidos para `.wem` via Wwise (a comunidade de BG3 tem guias detalhados do processo).

Deixe claro na UI que faixas externas perdem a dinâmica de intensidade. Ofereça os dois modos explicitamente: "faixa dinâmica" (nativa) e "loop simples" (externa).

**Nunca distribua a OST junto com o mod.** O usuário fornece os arquivos.

---

## Instalação (Boss Music Swapper)

**Pré-requisito:** UE4SS instalado em `Expedition 33\Sandfall\Binaries\Win64\`.

1. Extraia a pasta `BossMusicSwapper` para:
   ```
   Expedition 33\Sandfall\Binaries\Win64\ue4ss\Mods\BossMusicSwapper\
   ```
2. Inicie o jogo. **Nada muda ainda** — a configuração começa vazia.
3. Pressione a hotkey (padrão sugerido: **F9**) para abrir o menu.
4. Escolha um boss à esquerda, uma faixa à direita, e pronto.

Estrutura do pacote distribuído:

```
BossMusicSwapper/
├── dlls/main.dll
├── data/bosses.json
├── data/tracks.json
├── config.json          # { "enabled": true, "overrides": {} }
└── enabled.txt
```

---

# Ambiente de teste (vale para os dois)

Preparar isso antes economiza dezenas de horas:

1. **Save de desenvolvimento.** Baixe o save com todos os bosses de história e opcionais liberados e todos os pontos de viagem ativos. Faça backup dele. Transforma "preciso jogar 40 horas" em "carrego e viajo".
2. **Hot reload de config** em ambos os mods.
3. **Comando de teste** que simula o evento (início de combate com id arbitrário, ou recálculo com build fictícia), para validar a lógica sem entrar em combate de verdade.
4. **Modo janela sem borda** durante o desenvolvimento, para alternar rápido entre jogo e editor.
5. **Build de debug com console do UE4SS aberto**, para ver logs em tempo real.

---

# Riscos conhecidos

| Risco | Mitigação |
|---|---|
| Patch do jogo quebra os offsets | Isolar tudo que toca a memória em `Game/` e `Hooks/`; fixar a versão testada no README |
| M0 do otimizador falha (não achar os pictos na memória) | É o primeiro milestone justamente por isso; se falhar, o plano B é voltar ao parsing de save |
| Busca do otimizador trava o frame | Thread separada desde o início, não como otimização posterior |
| Conflito de hotkey com outros mods | Hotkey configurável desde o M1 |
| Fórmula de dano incorreta mina a confiança | Fixtures medidas in-game e taxa de erro publicada no README |

---

# Cronograma agregado

| Projeto | Estimativa |
|---|---|
| Boss Music Swapper (M0 a M5) | **3 a 4 semanas** |
| Picto Optimizer (M0 a M6) | **6 a 9 semanas** |

**Recomendação de ordem:** faça o Boss Music Swapper primeiro. Ele é menor, te ensina a stack de UE4SS e ImGui que o otimizador vai exigir, e entrega algo publicável rápido. O `e33-modkit` que sai dele é a base do segundo projeto.

---

# Antes do primeiro commit

Em cada repositório: licença (MIT), README com screenshot do overlay, versão do jogo testada, e CI rodando build. Mod sem screenshot no README não é instalado por ninguém.
