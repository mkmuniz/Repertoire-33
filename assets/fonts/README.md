# Fontes

**EB Garamond**, sob a SIL Open Font License 1.1 (ver `OFL.txt`).

## Por que esta e não a do jogo

A fonte do E33 é de terceiros e não pode ser redistribuída num mod. EB Garamond
é um old-style francês do século XVI, da mesma linhagem tipográfica que a
Belle Époque retoma — é o mais próximo que se pode empacotar legalmente.

Arquivo variável: o eixo de peso existe, mas o stb_truetype do ImGui renderiza
só a instância padrão (Regular). A hierarquia do overlay não depende de negrito;
vem de tamanho, caixa alta e espaçamento entre letras, que é como a tipografia
Art Nouveau de fato constrói ênfase.

## Trocar pela sua

Substitua `EBGaramond.ttf` mantendo o nome. Se o arquivo sumir, o overlay cai
na fonte padrão do ImGui e continua funcionando — só perde o estilo.
