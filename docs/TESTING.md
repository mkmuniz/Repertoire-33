# Como testar este mod

Duas metades: o que dá para verificar aqui, sem o jogo, e o que só o PC com o
jogo responde.

## No MacBook, agora

```sh
xmake f -y                                  # baixa nlohmann_json, doctest, imgui, glfw
xmake build tests && xmake run tests        # suíte de lógica
xmake build harness && xmake run harness    # o overlay numa janela nativa
```

### O que a suíte cobre

| Área | O que é verificado |
|---|---|
| `Support/` | escrita atômica, parse tolerante, arquivo ausente não é erro |
| `Config/Overrides` | config vazia, malformada, hot reload, presets, toggle global |
| `Config/Settings` | hotkey inválida rejeitada, escala limitada, versão antiga carrega |
| `Data/Catalog` | busca sem acento, id fora do catálogo, "vistos em jogo" |
| `Core/Swapper` | um motivo explícito para cada decisão de trocar ou não |
| `Core/ModController` | pipeline inteiro: evento → decisão → backend de áudio |

### O que fazer no harness

1. `xmake run harness` abre duas janelas: o overlay e o simulador de combate.
2. No simulador, escolha um encontro e clique **Iniciar combate**. Sem override
   configurado, nada toca — é o comportamento vanilla, e é o esperado.
3. No overlay, aba **Bosses**: selecione um encontro à esquerda, uma faixa à
   direita. Volte ao simulador e inicie o combate: a faixa aparece em "Faixas
   que o backend recebeu".
4. Desmarque **Overrides ativos** no topo e repita: nada toca, e a configuração
   continua lá.
5. Aba **Diagnostico**: cada combate observado, com o motivo de ter trocado ou
   não.
6. Clique **Encontro fora do catalogo**: o id aparece na lista "vistos em jogo",
   que é como os ids reais vão ser descobertos no PC.

## No PC com o jogo (o que falta)

Nesta ordem, porque cada um depende do anterior:

1. **M0** — instalar o hook de início de combate (`src/Hooks/CombatStart.cpp`).
   Pronto quando três bosses diferentes produzem três ids diferentes no log do
   UE4SS. Prototipar em Lua primeiro: recarrega sem fechar o jogo.
2. **Pré-requisito do overlay** — confirmar como a versão de UE4SS usada registra
   uma janela ImGui própria sobre o jogo. Hoje o mod registra uma aba
   (`register_tab`), que sempre funciona mas exige a janela do UE4SS aberta.
3. **M1** — preencher `data/bosses.json` e `data/tracks.json` com os ids reais,
   cruzando com `DT_jRPG_Encounters`. Os ids que faltarem aparecem no overlay.
4. **M2** — implementar `UnrealAudioBackend::play_instead` e `preview`.
   Pronto quando você entra no boss X e ouve a música do boss Y, com a dinâmica
   de intensidade intacta.
5. **Acentos** — verificar se "Sirène" aparece inteiro. Se sair cortado, é o
   glyph range da fonte do UE4SS, não o código do overlay.

Ambiente que economiza horas: save com todos os bosses liberados, modo janela
sem borda, e o console do UE4SS aberto com log verboso ligado nos Ajustes.
