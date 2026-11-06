# Como testar este mod

Duas metades: o que dá para verificar aqui, sem o jogo, e o que só o PC com o
jogo responde.

## No MacBook, agora

```sh
xmake f -y                                  # baixa nlohmann_json, doctest, imgui, glfw
xmake build tests && xmake run tests        # suíte de lógica
xmake build harness && xmake run harness    # o overlay numa janela nativa
./tools/check-windows.sh                    # sintaxe do código Windows (mingw)
```

### O que a suíte cobre

| Área | O que é verificado |
|---|---|
| `Support/` | escrita atômica, parse tolerante, arquivo ausente não é erro |
| `Config/Overrides` | config vazia, malformada, hot reload, presets, toggle global |
| `Config/Settings` | hotkey inválida rejeitada, escala limitada, versão antiga carrega |
| `Data/Catalog` | busca sem acento, id fora do catálogo, "vistos em jogo", o `data/` distribuído carrega sem id repetido |
| `Core/Swapper` | um motivo explícito para cada decisão de trocar ou não |
| `Core/ModController` | pipeline inteiro: evento → decisão → backend de áudio |
| `Unreal/` | FNamePool, GUObjectArray, FField/FProperty e regra de sufixo do FName, sobre memória montada com o layout do UE 5.4 |
| `Hooks/MusicPatcher` | a troca sobre um mundo sintético com as classes e campos do E33: troca, restauração, objeto recarregado, CDO intocado, faixa inexistente, início de combate, troca tardia |

O que a suíte **não** prova: que o jogo tem esses campos com esses nomes, e que
ele lê a música dali. Isso é a primeira sessão no PC.

### O que fazer no harness

1. `xmake run harness` abre duas janelas: o overlay e o simulador de combate.
2. No simulador, escolha um encontro e clique **Iniciar combate**. Sem override
   configurado, nada toca — é o comportamento vanilla, e é o esperado.
3. No overlay, aba **Encontros**: selecione um encontro à esquerda, uma faixa à
   direita. Volte ao simulador e inicie o combate: a faixa aparece em "Faixas
   que o backend recebeu".
4. Desmarque **Overrides ativos** no topo e repita: nada toca, e a configuração
   continua lá.
5. Aba **Diagnostico**: cada combate observado, com o motivo de ter trocado ou
   não.

## No PC com o jogo

Instale como diz o README e abra o jogo. O log fica em
`Win64\Repertoire33\BossMusicSwapper.log`. Ligue **Log verboso** na aba
Diagnostico do overlay (F9) para ver também cada fonte de música encontrada.

### 1. A integração subiu?

Procure no log, nesta ordem:

```
janela do jogo encontrada; tick da game thread ativo
Unreal encontrado: <N> objetos (item de 24 bytes), GUObjectArray+0x10 em RVA 0x..., FNamePool em RVA 0x...
```

No overlay, aba Diagnostico, a linha "Integracao com o jogo" deve dizer
`ativa: X inimigo(s), Y acao(oes) de roteiro, 1 battle manager`. Com um save
carregado, X e Y passam de zero.

Se parar antes, a própria linha diz onde. As mensagens possíveis:

| Mensagem | Significa |
|---|---|
| `procurando o pool de nomes` / `a tabela de objetos` | o jogo ainda está abrindo; se não passar disso, o executável mudou de forma que a busca estrutural não reconhece |
| `layout do Unreal diferente do esperado: ...` | um patch mudou o engine; o mod ficou vanilla de propósito. O texto diz qual campo |
| `classe ...: campo ... nao confere` (no log) | o jogo renomeou um campo de Blueprint; só aquela fonte foi ignorada |

### 2. Teste rápido com um inimigo comum

Mais rápido que ir até um boss:

1. Entre em qualquer luta. O log diz `combate iniciado: <id> (faixa original: <caminho>)`.
2. Fuja ou termine a luta. O id aparece no overlay em "Vistos em jogo" (ou já
   está na lista, se for um boss).
3. Selecione esse encontro e escolha outra faixa, de preferência uma bem
   diferente (um tema de boss).
4. Volte a lutar com o mesmo tipo de inimigo. Devem aparecer no log:

   ```
   <id>: <faixa original> -> <faixa escolhida> (<nome do ator>)
   combate iniciado: <id> ...
   musica da batalha carregada: <faixa escolhida>
   ```

   e a música da luta é a escolhida.

5. Clique no `x` da linha (voltar ao original) e lute de novo: a original volta,
   e o log diz `original restaurada`.

### 3. Boss

Escolha um boss na lista (os 122 de `data/bosses.json`), atribua uma faixa e
entre na luta. Mesmo critério: `musica da batalha carregada` com a faixa
escolhida, e a música certa tocando.

### Se não trocar

O que mais ajuda é o trecho do log entre `combate iniciado` e
`musica da batalha carregada` dessa luta, com o log verboso ligado. Três casos:

| O log mostra | Leitura |
|---|---|
| `musica da batalha carregada` com a faixa **original**, sem `troca tardia` antes | o jogo pegou a música de outra fonte; o próximo passo é interceptar o carregamento (o callback `OnLoaded_*` do battle manager) |
| `troca tardia na batalha em curso` e mesmo assim a original carregada | a música é pré-carregada antes do início detectável; idem |
| `faixa ... nao existe no jogo` | o caminho no catálogo está errado para esta versão do jogo |

Ambiente que economiza horas: save com todos os bosses liberados, modo janela
sem borda, e o log verboso ligado.
