# Projeto 1 - Computação Visual - Processamento de Imagens com SDL e Linguagem C

## Integrantes
- Fabio Oliveira da Silva - 10420458
- Patrick Rocha de Andrade - 10410902

## Descrição do projeto
O projeto foi baseado em alguns objetivos principais, sendo eles:
1. Informar se uma imagem é colorida ou está em escala de cinza.
2. Transformar a imagem em escala de cinza.
3. Mostrar o histograma da imagem em uma janela secundária, além de informações como desvio padrão e média de intensidade.
4. Equalizar o histograma da imagem e aplicar a transformação na imagem.
5. Mostrar a imagem em resolução original ou no padrão definido no escopo do projeto (1024x768)
6. Salvar a imagem tratada apertando 'S'.

## Funcionamento do Programa 
### Janelas e controles

- **Janela principal**: exibe a imagem sendo processada (original em escala
  de cinza ou equalizada, na resolução original ou em 1024x768, conforme os
  botões da janela secundária).
- **Janela secundária** (posicionada em (0,0) da tela, filha da janela
  principal): exibe
  - o histograma da imagem atual;
  - a intensidade média e sua classificação (clara/média/escura);
  - o desvio padrão e a classificação de contraste (alto/médio/baixo);
  - botão **Equalizar / Ver original**: alterna entre a imagem original em
    escala de cinza e sua versão equalizada;
  - botão **Resolução original / 1024x768**: alterna a resolução de exibição
    da imagem na janela principal.
- **Tecla S**: salva a imagem atualmente exibida em `output_image.png` no
  diretório de execução do programa (sobrescrevendo se já existir).

### Fonte utilizada

Os textos do histograma e dos botões são renderizados com a biblioteca
SDL_ttf, carregando um arquivo `.ttf` do disco a partir da constante
`FONT_FILENAME`, definida no topo de `main.c`. Estamos usando a fonte
**[DejaVu Sans]**.

**Importante:** o arquivo `.ttf` correspondente precisa estar na mesma pasta
do executável no momento da execução (ou o caminho em `FONT_FILENAME` deve
ser ajustado).


## Ambiente de testes utilizado
A estrutura do ambiente usado para testar o código foi:
- SO: WSL com Ubuntu na versão 26.04
- GCC versão 15.2.0
- SDL e SDL_Image na versão 3.4.0
- SDL_ttf na versão 3.2.0

## Funcionamento

### Compilação
Para compilar o código, é necessário que o arquivo com a fonte .ttf esteja no mesmo diretório 
que o executável será salvo. Para compilar o main.c pelo terminal do Linux, o seguinte o
seguinte comando deve ser executado:
```
gcc -std=c99 main.c -o programa -lSDL3 -lSDL3_image -lSDL3_ttf -lm
```
e, logo após, o seguinte:

```
./programa caminho_da_imagem.ext
```

Sendo `programa` o executável gerado na compilação e `caminho_da_imagem.ext`
o caminho de um arquivo de imagem (ex.: `.png`, `.jpg`, `.bmp`).



## Uso de IA generativa
IAs generativas foram usadas para ajudar a configurar o ambiente em que o 
código seria implementado. Além disso, foram usadas para ajudar a entender o projeto, 
o que seria necessário implementar/estruturar e como o fazer em algumas partes 
que nos geraram dúvidas (principalmente a manipulação de duas janelas e do histograma). 

## Contribuições de cada integrante
- **Fabio Oliveira da Silva**: testes do programa no ambiente
  (compilação, execução e testes) e algumas funções como: carregamento de
  janela/renderer (`MyWindow_initialize`, `MyWindow_destroy`,
  `MyImage_destroy`), carregamento de imagem (`load_rgba32`), detecção e
  conversão para escala de cinza (`escala_cinza`, `converte_cinza`),
  salvamento da imagem (`salvar_imagem`) e cálculo do histograma e das
  estatísticas da imagem (`calcular_histograma`, `media_desvio_padrao`, `brilho_class`,
  `contraste_class`,  `atualizar_histograma`).
- **Patrick Rocha de Andrade**: Responsável pelas funções de equalização de imagem
  (`equalizar_imagem`), a interface gráfica com as duas janelas e o sistema
  de botões com estados neutro/hover/pressionado (`renderizar_botao`,
  `layout_secondary_window_widgets`, `toggle_equalize`,
  `toggle_resolution`), a renderização de texto com SDL_ttf
  (`renderizar_texto`) e a alternância de resolução com reposicionamento de
  janela (`apply_display_mode`, `reposition_main_window`).

