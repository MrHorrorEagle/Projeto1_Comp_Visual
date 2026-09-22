// Baseado em código de Andre Kishimoto - https://kishimoto.com.br/
//------------------------------------------------------------------------------
// PROJETO 1 - COMPUTAÇÃO VISUAL
// Processamento de imagens em C com SDL
//  Fabio Oliveira da Silva - 10420458
//  Patrick Rocha de Andrade - 10410902
//
// Resumo das funcionalidades implementadas:
// 1. Carregamento de imagem (com tratamento de erro).
// 2. Detecção de imagem colorida/escala de cinza + conversão (fórmula
//    Y = 0.2125*R + 0.7154*G + 0.0721*B).
// 3. Janela principal (exibe a imagem) + janela secundária filha (histograma,
//    informações e botões).
// 4. Cálculo e exibição do histograma, média de intensidade e desvio padrão.
// 5. Equalização de histograma (com toggle para voltar à imagem original).
// 6. Alternância entre resolução original e 1024x768.
// 7. Salvar imagem atual com a tecla S (output_image.png).
// 8. Exibição de texto usando SDL_ttf, carregando uma fonte .ttf externa
//    (caminho definido na constante FONT_FILENAME, para facilitar troca).
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// bibliotecas usadas
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

//------------------------------------------------------------------------------
// Constantes e enums
//------------------------------------------------------------------------------

// Caminho da fonte TTF usada para desenhar textos (histograma e botões).
// Estamos considerando que o arquivo .ttf está no mesmo diretório do executável  
static const char *FONT_FILENAME = "DejaVuSans.ttf";
static const int FONT_SIZE = 16;

static const char *MAIN_WINDOW_TITLE = "Projeto 1 - Processamento de Imagens";
static const char *SECONDARY_WINDOW_TITLE = "Histograma";

static const char *OUTPUT_IMAGE_FILENAME = "output_image.png";

enum constants
{
  // Tamanho inicial: 1024x768 da janela principal
  MAIN_WINDOW_WIDTH = 1024,
  MAIN_WINDOW_HEIGHT = 768,

  // Tamanho fixo da janela secundária
  SECONDARY_WINDOW_WIDTH = 360,
  SECONDARY_WINDOW_HEIGHT = 560,

  // Quantidade de níveis de intensidade (imagem em escala de cinza, 8 bits).
  HISTOGRAMA_INTENSIDADE = 256,
  COR_MAX = 255,

  // Layout da janela secundária (em pixels).
  MARGIN = 20,
  HISTOGRAM_HEIGHT = 180,
  TEXT_LINE_HEIGHT = 22,
  BUTTON_HEIGHT = 44,
  BUTTON_SPACING = 14,

  BUTTON_TEXT_MAX_LENGTH = 32,
};

// Estado visual de um botão, conforme pedido no enunciado (itens 5 e 6):
// cor "neutra", cor quando o mouse está em cima e cor quando está pressionado.
typedef enum ButtonState
{
  BUTTON_STATE_NEUTRAL,
  BUTTON_STATE_HOVER,
  BUTTON_STATE_PRESSED,
} ButtonState;

// Identificador de cada botão da janela secundária, usado para saber qual
// ação executar quando o clique é confirmado (mouse solto em cima do botão).
typedef enum BotaoID
{
  BOTAO_EQUALIZAR,
  BOTAO_RESOLUCAO,
} BotaoID;

typedef struct MyWindow MyWindow;
struct MyWindow
{
  SDL_Window *window;
  SDL_Renderer *renderer;
};

// Representa uma imagem carregada/gerada pelo programa: a superfície com os
// pixels (necessária para cálculos como histograma/equalização), a textura
// (usada para desenhar na tela) e o retângulo com as dimensões originais.
typedef struct MyImage MyImage;
struct MyImage
{
  SDL_Surface *surface;
  SDL_Texture *texture;
  SDL_FRect rect;
};

// Struct para representar botões
typedef struct MyButton MyButton;
struct MyButton
{
  BotaoID id;
  SDL_FRect rect;
  char text[BUTTON_TEXT_MAX_LENGTH];
  ButtonState state;
};

//------------------------------------------------------------------------------
// Globais (argh!)
//------------------------------------------------------------------------------
static MyWindow g_mainWindow = { .window = NULL, .renderer = NULL };
static MyWindow g_secondaryWindow = { .window = NULL, .renderer = NULL };

static TTF_Font *g_font = NULL;

// g_imageGrayscale: imagem original, já convertida para escala de cinza (ou
// já era escala de cinza), nunca é modificada depois de carregada.
// g_imageEqualized: versão equalizada de g_imageGrayscale, calculada uma
// única vez (permite alternar entre as duas sem recarregar a imagem).
static MyImage g_imageGrayscale = { .surface = NULL, .texture = NULL, .rect = { 0.0f, 0.0f, 0.0f, 0.0f } };
static MyImage g_imageEqualized = { .surface = NULL, .texture = NULL, .rect = { 0.0f, 0.0f, 0.0f, 0.0f } };

// Aponta para a imagem que deve ser exibida/salva no momento (uma das duas
// acima). Começa apontando para a imagem em escala de cinza (não equalizada).
static MyImage *g_currentImage = &g_imageGrayscale;

// Estado dos toggles controlados pelos botões da janela secundária.
static bool g_isEqualized = false;         // false = mostrando a original em escala de cinza
static bool g_isOriginalResolution = false; // false = mostrando no modo 1024x768 (padrão inicial)

static MyButton g_buttonEqualize = { .id = BOTAO_EQUALIZAR, .state = BUTTON_STATE_NEUTRAL };
static MyButton g_buttonResolution = { .id = BOTAO_RESOLUCAO, .state = BUTTON_STATE_NEUTRAL };

// Histograma (256 posições) e estatísticas da imagem atualmente exibida.
// Recalculados sempre que a imagem exibida muda (troca entre original e
// equalizada).
static int g_histograma[HISTOGRAMA_INTENSIDADE];
static double g_histogramamedia = 0.0;
static double g_histogramadesvio_padrao = 0.0;

// Layout da janela secundária: calculado uma única vez por
// layout_secondary_window_widgets() e reaproveitado tanto para desenhar
// quanto para detectar cliques nos botões. Mantemos tudo em um só lugar para
// que os elementos nunca fiquem desalinhados ou se sobreponham entre si.
static SDL_FRect g_histogramaArea = { 0.0f, 0.0f, 0.0f, 0.0f };
static float g_statsLine1Y = 0.0f;
static float g_statsLine2Y = 0.0f;

//------------------------------------------------------------------------------
// Declaração de funções
//------------------------------------------------------------------------------
// Funções do exemplo de inverter imagem 
static bool MyWindow_initialize(MyWindow *window, const char *title, int width, int height, SDL_WindowFlags window_flags);
static void MyWindow_destroy(MyWindow *window);
static void MyImage_destroy(MyImage *image);
static bool load_rgba32(const char *filename, SDL_Renderer *renderer, MyImage *output_image);

// Funções readaptadas 
static bool escala_cinza(SDL_Surface *surface);
static void converte_cinza(SDL_Surface *surface);

// Funções novas adicionadas para o projeto
// IA generativa nos ajudou a entender o que precisava ser feito e nos ajudou a estruturar
// algumas funções.
static Uint8 clamp_to_uint8(double value);
static void calcular_histograma(SDL_Surface *surface, int histograma[HISTOGRAMA_INTENSIDADE]);
static void media_desvio_padrao(const int histograma[HISTOGRAMA_INTENSIDADE], size_t qtdPixel, double *pont_media, double *pont_desvio_padrao);
static const char *brilho_class(double media);
static const char *contraste_class(double desvio_padrao);
static void atualizar_histograma(void);

static bool equalizar_imagem(SDL_Renderer *renderer, const MyImage *source, MyImage *output_image);

static bool point_in_rect(float x, float y, const SDL_FRect *rect);
static void layout_secondary_window_widgets(void);
static void renderizar_texto(SDL_Renderer *renderer, const char *text, float x, float y, SDL_Color color);
static void renderizar_histograma(SDL_Renderer *renderer, const SDL_FRect *area);
static void renderizar_botao(SDL_Renderer *renderer, const MyButton *button);

static void reposition_main_window(int width, int height);
static void apply_display_mode(void);
static void toggle_equalize(void);
static void toggle_resolution(void);
static void salvar_imagem(void);

static void renderizar_tela_princ(void);
static void renderizar_tela_seg(void);

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool MyWindow_initialize(MyWindow *window, const char *title, int width, int height, SDL_WindowFlags window_flags)
{
  SDL_Log("\tMyWindow_initialize(%s, %d, %d)", title, width, height);

  if (!window)
  {
    SDL_Log("\t\t*** Erro: Janela/renderizador inválidos (window == NULL).");
    return false;
  }

  return SDL_CreateWindowAndRenderer(title, width, height, window_flags, &window->window, &window->renderer);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyWindow_destroy(MyWindow *window)
{
  SDL_Log(">>> MyWindow_destroy()");

  if (!window)
  {
    SDL_Log("\t*** Erro: Janela/renderizador inválidos (window == NULL).");
    SDL_Log("<<< MyWindow_destroy()");
    return;
  }

  SDL_Log("\tDestruindo MyWindow->renderer...");
  SDL_DestroyRenderer(window->renderer);
  window->renderer = NULL;

  SDL_Log("\tDestruindo MyWindow->window...");
  SDL_DestroyWindow(window->window);
  window->window = NULL;

  SDL_Log("<<< MyWindow_destroy()");
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyImage_destroy(MyImage *image)
{
  SDL_Log(">>> MyImage_destroy()");

  if (!image)
  {
    SDL_Log("\t*** Erro: Imagem inválida (image == NULL).");
    SDL_Log("<<< MyImage_destroy()");
    return;
  }

  if (image->texture)
  {
    SDL_Log("\tDestruindo MyImage->texture...");
    SDL_DestroyTexture(image->texture);
    image->texture = NULL;
  }

  if (image->surface)
  {
    SDL_Log("\tDestruindo MyImage->surface...");
    SDL_DestroySurface(image->surface);
    image->surface = NULL;
  }

  image->rect.x = image->rect.y = image->rect.w = image->rect.h = 0.0f;

  SDL_Log("<<< MyImage_destroy()");
}

//------------------------------------------------------------------------------
// Carrega a imagem indicada em `filename`, converte para RGBA32 (elimina
// dependência do formato original) e cria a textura correspondente.
// Retorna false em caso de erro (arquivo não encontrado, formato inválido,
// etc.), sempre imprimindo uma mensagem pertinente no terminal (item 1 do
// enunciado).
//------------------------------------------------------------------------------
bool load_rgba32(const char *filename, SDL_Renderer *renderer, MyImage *output_image)
{
  SDL_Log(">>> load_rgba32(\"%s\")", filename);

  if (!filename || !renderer || !output_image)
  {
    SDL_Log("\t*** Erro: Parâmetros inválidos para load_rgba32().");
    SDL_Log("<<< load_rgba32(\"%s\")", filename);
    return false;
  }

  MyImage_destroy(output_image);

  SDL_Log("\tCarregando imagem \"%s\" em uma superfície...", filename);
  SDL_Surface *surface = IMG_Load(filename);
  if (!surface)
  {
    // Cobre tanto "arquivo não encontrado" quanto "formato inválido", já
    // que o SDL_image tenta identificar o formato pelo conteúdo do arquivo.
    SDL_Log("\t*** Erro: não foi possível abrir \"%s\" como imagem (arquivo inexistente ou formato inválido). Detalhe da SDL: %s", filename, SDL_GetError());
    SDL_Log("<<< load_rgba32(\"%s\")", filename);
    return false;
  }

  SDL_Log("\tConvertendo superfície para formato RGBA32...");
  output_image->surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(surface);
  if (!output_image->surface)
  {
    SDL_Log("\t*** Erro ao converter superfície para formato RGBA32: %s", SDL_GetError());
    SDL_Log("<<< load_rgba32(\"%s\")", filename);
    return false;
  }

  SDL_Log("\tCriando textura a partir da superfície...");
  output_image->texture = SDL_CreateTextureFromSurface(renderer, output_image->surface);
  if (!output_image->texture)
  {
    SDL_Log("\t*** Erro ao criar textura: %s", SDL_GetError());
    SDL_Log("<<< load_rgba32(\"%s\")", filename);
    return false;
  }

  SDL_Log("\tObtendo dimensões da textura...");
  SDL_GetTextureSize(output_image->texture, &output_image->rect.w, &output_image->rect.h);

  SDL_Log("<<< load_rgba32(\"%s\")", filename);
  return true;
}

//------------------------------------------------------------------------------
// Verifica se todos os pixels da superfície têm R == G == B, ou seja, se a
// imagem já está em escala de cinza. Assumimos superfície no formato RGBA32
// (garantido por load_rgba32).
//------------------------------------------------------------------------------
bool escala_cinza(SDL_Surface *surface)
{
  if (!surface) return false;

  SDL_LockSurface(surface);

  const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
  const size_t qtdPixel = (size_t)surface->w * (size_t)surface->h;
  Uint32 *pixels = (Uint32 *)surface->pixels;

  bool result = true;
  Uint8 r = 0, g = 0, b = 0, a = 0;

  for (size_t i = 0; i < qtdPixel; ++i)
  {
    SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);
    if (r != g || g != b)
    {
      result = false;
      break;
    }
  }

  SDL_UnlockSurface(surface);
  return result;
}

//------------------------------------------------------------------------------
// Converte a superfície colorida para escala de cinza, aplicando a fórmula
// pedida no enunciado: Y = 0.2125*R + 0.7154*G + 0.0721*B.
// O canal Alpha não é alterado. A superfície é modificada em memória.
//------------------------------------------------------------------------------
void converte_cinza(SDL_Surface *surface)
{
  SDL_Log(">>> converte_cinza()");

  if (!surface)
  {
    SDL_Log("\t*** Erro: Superfície inválida (surface == NULL).");
    SDL_Log("<<< converte_cinza()");
    return;
  }

  SDL_LockSurface(surface);

  const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
  const size_t qtdPixel = (size_t)surface->w * (size_t)surface->h;
  Uint32 *pixels = (Uint32 *)surface->pixels;

  Uint8 r = 0, g = 0, b = 0, a = 0;

  for (size_t i = 0; i < qtdPixel; ++i)
  {
    SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

    double y = (0.2125 * r) + (0.7154 * g) + (0.0721 * b);
    Uint8 gray = clamp_to_uint8(y);

    pixels[i] = SDL_MapRGBA(format, NULL, gray, gray, gray, a);
  }

  SDL_UnlockSurface(surface);

  SDL_Log("<<< converte_cinza()");
}

//------------------------------------------------------------------------------
// Limita um valor double ao intervalo [0, 255] e arredonda para o inteiro
// mais próximo, retornando um Uint8. Usado em várias contas (conversão para
// escala de cinza, equalização, etc.) para evitar estouro/valores inválidos.
//------------------------------------------------------------------------------
Uint8 clamp_to_uint8(double value)
{
  if (value < 0.0) return 0;
  if (value > 255.0) return 255;
  return (Uint8)(value + 0.5);
}

//------------------------------------------------------------------------------
// Calcula o histograma (256 posições, uma por nível de intensidade) da
// superfície informada. Assumimos escala de cinza, então basta olhar o canal
// R de cada pixel (R == G == B).
//------------------------------------------------------------------------------
void calcular_histograma(SDL_Surface *surface, int histograma[HISTOGRAMA_INTENSIDADE])
{
  memset(histograma,0, sizeof(int) * HISTOGRAMA_INTENSIDADE);

  if (!surface) return;

  SDL_LockSurface(surface);

  const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
  const size_t qtdPixel = (size_t)surface->w * (size_t)surface->h;
  Uint32 *pixels = (Uint32 *)surface->pixels;

  Uint8 r = 0, g = 0, b = 0, a = 0;

  for (size_t i = 0; i < qtdPixel; ++i)
  {
    SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);
    histograma[r]++;
  }

  SDL_UnlockSurface(surface);
}

//------------------------------------------------------------------------------
// A partir do histograma, calcula a média de intensidade e o desvio padrão
// (formulação estatística padrão), que serão usados para classificar a
// imagem.
//------------------------------------------------------------------------------
void media_desvio_padrao(const int histograma[HISTOGRAMA_INTENSIDADE], size_t qtdPixel, double *pont_media, double *pont_desvio_padrao)
{
  if (qtdPixel == 0)
  {
    *pont_media = 0.0;
    *pont_desvio_padrao = 0.0;
    return;
  }

  double soma= 0.0;
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    soma+= (double)i * (double)histograma[i];
  }
  double media = soma/ (double)qtdPixel;

  double raiz_quad_soma= 0.0;
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    double diff = (double)i - media;
    raiz_quad_soma+= (diff * diff) * (double)histograma[i];
  }
  double variancia = raiz_quad_soma/ (double)qtdPixel;

  *pont_media = media;
  *pont_desvio_padrao = sqrt(variancia);
}

//------------------------------------------------------------------------------
// Classifica a imagem em "clara", "média" ou "escura" a partir da média de
// intensidade (0-255). Os limites abaixo dividem o intervalo em terços; são
// uma escolha do grupo e podem ser ajustados/justificados no relatório.
//------------------------------------------------------------------------------
const char *brilho_class(double media)
{
  if (media < 85.0) return "escura";
  if (media <= 170.0) return "media";
  return "clara";
}

//------------------------------------------------------------------------------
// Classifica o contraste da imagem em "alto", "medio" ou "baixo" a partir do
// desvio padrão. Assim como a classificação de brilho, os limites são uma
// escolha heurística do grupo (documentar/justificar no relatório).
//------------------------------------------------------------------------------
const char *contraste_class(double desvio_padrao)
{
  if (desvio_padrao < 40.0) return "baixo";
  if (desvio_padrao <= 80.0) return "medio";
  return "alto";
}

//------------------------------------------------------------------------------
// Recalcula o histograma e as estatísticas (média/desvio padrão) a partir da
// imagem atualmente exibida (g_currentImage). Deve ser chamada sempre que
// g_currentImage mudar (ex.: ao equalizar ou reverter para a original).
//------------------------------------------------------------------------------
void atualizar_histograma(void)
{
  calcular_histograma(g_currentImage->surface, g_histograma);

  size_t qtdPixel = (size_t)g_currentImage->surface->w * (size_t)g_currentImage->surface->h;
  media_desvio_padrao(g_histograma, qtdPixel, &g_histogramamedia, &g_histogramadesvio_padrao);
}

//------------------------------------------------------------------------------
// Gera uma nova imagem equalizada a partir da imagem em escala de cinza
// `source`, usando o algoritmo clássico de equalização de histograma baseado
// na função de distribuição acumulada (CDF):
//   1. histograma da imagem de entrada;
//   2. CDF (soma acumulada do histograma);
//   3. novo valor de cada nível i = round((cdf[i] - cdf_min) / (N - cdf_min) * 255)
//   4. aplica o mapeamento pixel a pixel.
// O resultado é armazenado em `output_image` (superfície + textura próprias,
// independentes de `source`).
//
// IA generativa nos ajudou a entender o que precisava ser feito e nos ajudou a estruturar essa função 
//------------------------------------------------------------------------------
bool equalizar_imagem(SDL_Renderer *renderer, const MyImage *source, MyImage *output_image)
{
  SDL_Log(">>> equalizar_imagem()");

  if (!renderer || !source || !source->surface || !output_image)
  {
    SDL_Log("\t*** Erro: Parâmetros inválidos para equalizar_imagem().");
    SDL_Log("<<< equalizar_imagem()");
    return false;
  }

  // Histograma da imagem de origem (a original em escala de cinza).
  int histograma[HISTOGRAMA_INTENSIDADE];
  calcular_histograma(source->surface, histograma);

  size_t qtdPixel = (size_t)source->surface->w * (size_t)source->surface->h;

  // Função de distribuição acumulada (CDF).
  long cdf[HISTOGRAMA_INTENSIDADE];
  cdf[0] = histograma[0];
  for (int i = 1; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    cdf[i] = cdf[i - 1] + histograma[i];
  }

  // Primeiro valor não-nulo da CDF (necessário na fórmula de equalização).
  long cdfMin = 0;
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    if (cdf[i] != 0)
    {
      cdfMin = cdf[i];
      break;
    }
  }

  // Tabela de mapeamento (nível antigo -> nível equalizado).
  Uint8 lookupTable[HISTOGRAMA_INTENSIDADE];
  long denominator = (long)qtdPixel - cdfMin;
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    if (denominator <= 0)
    {
      // Caso degenerado: imagem com um único nível de intensidade.
      lookupTable[i] = (Uint8)i;
    }
    else
    {
      double newValue = ((double)(cdf[i] - cdfMin) / (double)denominator) * 255.0;
      lookupTable[i] = clamp_to_uint8(newValue);
    }
  }

  // Duplica a superfície de origem e aplica a tabela de mapeamento em cada
  // pixel (mantendo R == G == B, já que a imagem é em escala de cinza).
  MyImage_destroy(output_image);
  output_image->surface = SDL_DuplicateSurface(source->surface);
  if (!output_image->surface)
  {
    SDL_Log("\t*** Erro ao duplicar superfície: %s", SDL_GetError());
    SDL_Log("<<< equalizar_imagem()");
    return false;
  }

  SDL_LockSurface(output_image->surface);

  const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(output_image->surface->format);
  Uint32 *pixels = (Uint32 *)output_image->surface->pixels;
  Uint8 r = 0, g = 0, b = 0, a = 0;

  for (size_t i = 0; i < qtdPixel; ++i)
  {
    SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);
    Uint8 newLevel = lookupTable[r];
    pixels[i] = SDL_MapRGBA(format, NULL, newLevel, newLevel, newLevel, a);
  }

  SDL_UnlockSurface(output_image->surface);

  output_image->texture = SDL_CreateTextureFromSurface(renderer, output_image->surface);
  if (!output_image->texture)
  {
    SDL_Log("\t*** Erro ao criar textura da imagem equalizada: %s", SDL_GetError());
    SDL_Log("<<< equalizar_imagem()");
    return false;
  }

  SDL_GetTextureSize(output_image->texture, &output_image->rect.w, &output_image->rect.h);

  SDL_Log("<<< equalizar_imagem()");
  return true;
}

//------------------------------------------------------------------------------
// Retorna true se o ponto (x, y) está dentro do retângulo informado. Usado
// para detectar hover/clique nos botões da janela secundária.
//------------------------------------------------------------------------------
bool point_in_rect(float x, float y, const SDL_FRect *rect)
{
  return (x >= rect->x) && (x <= rect->x + rect->w) &&
         (y >= rect->y) && (y <= rect->y + rect->h);
}

//------------------------------------------------------------------------------
// Define a posição/tamanho dos dois botões da janela secundária, além de
// atualizar o texto de cada um de acordo com o estado atual (equalizado ou
// não, resolução original ou 1024x768). Chamada sempre que o layout precisa
// ser (re)calculado ou o texto de um botão precisa mudar.
//------------------------------------------------------------------------------
void layout_secondary_window_widgets(void)
{
  float contentWidth = (float)(SECONDARY_WINDOW_WIDTH - (2 * MARGIN));

  // Calculamos a posição de CADA elemento em sequência, de cima para baixo,
  // sempre a partir do fim do elemento anterior + um espaçamento. Assim,
  // nenhum elemento pode "nascer" sobrepondo o anterior: se algo mudar de
  // tamanho, os elementos abaixo dele se movem juntos automaticamente.

  // 1) Título "Histograma" (uma linha de texto).
  float titleY = (float)MARGIN;
  float titleBottom = titleY + (float)TEXT_LINE_HEIGHT;

  // 2) Área do histograma, logo abaixo do título.
  g_histogramaArea = (SDL_FRect){
    .x = (float)MARGIN,
    .y = titleBottom,
    .w = contentWidth,
    .h = (float)HISTOGRAM_HEIGHT,
  };
  float histogramBottom = g_histogramaArea.y + g_histogramaArea.h;

  // 3) Duas linhas de estatísticas (média e desvio padrão), abaixo do
  //    histograma, com um pequeno espaçamento (BUTTON_SPACING) entre elas.
  g_statsLine1Y = histogramBottom + (float)BUTTON_SPACING;
  g_statsLine2Y = g_statsLine1Y + (float)TEXT_LINE_HEIGHT;
  float statsBottom = g_statsLine2Y + (float)TEXT_LINE_HEIGHT;

  // 4) Botões, abaixo das estatísticas, com uma margem maior antes do
  //    primeiro botão para deixar claro que é um novo grupo de elementos.
  float buttonY = statsBottom + (float)MARGIN;

  g_buttonEqualize.rect = (SDL_FRect){
    .x = (float)MARGIN,
    .y = buttonY,
    .w = contentWidth,
    .h = (float)BUTTON_HEIGHT,
  };
  snprintf(g_buttonEqualize.text, BUTTON_TEXT_MAX_LENGTH, "%s", g_isEqualized ? "Ver original" : "Equalizar");

  g_buttonResolution.rect = (SDL_FRect){
    .x = (float)MARGIN,
    .y = buttonY + (float)BUTTON_HEIGHT + (float)BUTTON_SPACING,
    .w = contentWidth,
    .h = (float)BUTTON_HEIGHT,
  };
  snprintf(g_buttonResolution.text, BUTTON_TEXT_MAX_LENGTH, "%s", g_isOriginalResolution ? "1024x768" : "Resolucao original");
}

//------------------------------------------------------------------------------
// Desenha um texto na posição (x, y) usando a fonte global g_font. Cria uma
// superfície/textura temporárias, desenha e libera em seguida (o texto muda
// com frequência, então não vale a pena cachear a textura).
//------------------------------------------------------------------------------
void renderizar_texto(SDL_Renderer *renderer, const char *text, float x, float y, SDL_Color color)
{
  if (!g_font || !text || text[0] == '\0') return;

  SDL_Surface *textSurface = TTF_RenderText_Blended(g_font, text, strlen(text), color);
  if (!textSurface)
  {
    SDL_Log("\t*** Erro ao renderizar texto \"%s\": %s", text, SDL_GetError());
    return;
  }

  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
  if (!textTexture)
  {
    SDL_Log("\t*** Erro ao criar textura de texto: %s", SDL_GetError());
    SDL_DestroySurface(textSurface);
    return;
  }

  SDL_FRect destRect = { .x = x, .y = y, .w = (float)textSurface->w, .h = (float)textSurface->h };
  SDL_RenderTexture(renderer, textTexture, NULL, &destRect);

  SDL_DestroyTexture(textTexture);
  SDL_DestroySurface(textSurface);
}

//------------------------------------------------------------------------------
// Desenha o histograma (g_histograma) como barras verticais dentro da área
// informada, usando primitivas da SDL (SDL_RenderFillRect).
//------------------------------------------------------------------------------
void renderizar_histograma(SDL_Renderer *renderer, const SDL_FRect *area)
{
  int maxCount = 1; // evita divisão por zero
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    if (g_histograma[i] > maxCount) maxCount = g_histograma[i];
  }

  // Contorno da área do histograma, só para referência visual.
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, COR_MAX);
  SDL_RenderRect(renderer, area);

  float binWidth = area->w / (float)HISTOGRAMA_INTENSIDADE;

  SDL_SetRenderDrawColor(renderer, 220, 220, 220, COR_MAX);
  for (int i = 0; i < HISTOGRAMA_INTENSIDADE; ++i)
  {
    float barHeight = ((float)g_histograma[i] / (float)maxCount) * area->h;

    SDL_FRect bar = {
      .x = area->x + ((float)i * binWidth),
      .y = area->y + (area->h - barHeight),
      .w = binWidth,
      .h = barHeight,
    };
    SDL_RenderFillRect(renderer, &bar);
  }
}

//------------------------------------------------------------------------------
// Desenha um botão (retângulo colorido conforme o estado + texto centralizado
// aproximadamente), usando primitivas da SDL, conforme pedido nos itens 5 e 6
// do enunciado.
//------------------------------------------------------------------------------
void renderizar_botao(SDL_Renderer *renderer, const MyButton *button)
{
  SDL_Color fillColor;
  switch (button->state)
  {
    case BUTTON_STATE_HOVER:
      fillColor = (SDL_Color){ 90, 150, 255, COR_MAX };   // azul claro
      break;
    case BUTTON_STATE_PRESSED:
      fillColor = (SDL_Color){ 10, 40, 140, COR_MAX };    // azul escuro
      break;
    case BUTTON_STATE_NEUTRAL:
    default:
      fillColor = (SDL_Color){ 40, 90, 220, COR_MAX };    // azul "neutro"
      break;
  }

  SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
  SDL_RenderFillRect(renderer, &button->rect);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, COR_MAX);
  SDL_RenderRect(renderer, &button->rect);

  // Centralização aproximada do texto (sem medir a largura exata do texto
  // renderizado, para manter o código simples).
  float textX = button->rect.x + 12.0f;
  float textY = button->rect.y + (button->rect.h / 2.0f) - ((float)FONT_SIZE / 2.0f);
  renderizar_texto(renderer, button->text, textX, textY, (SDL_Color){ 255, 255, 255, COR_MAX });
}

//------------------------------------------------------------------------------
// Redimensiona e reposiciona a janela principal de acordo com `width` e
// `height`: centralizada no monitor principal, exceto se o tamanho exceder a
// resolução atual da tela, caso em que o canto superior esquerdo vai para
// (0,0) (regra do item 6 do enunciado).
//------------------------------------------------------------------------------
void reposition_main_window(int width, int height)
{
  SDL_SetWindowSize(g_mainWindow.window, width, height);

  SDL_DisplayID display = SDL_GetPrimaryDisplay();
  SDL_Rect displayBounds = { 0, 0, 0, 0 };
  SDL_GetDisplayBounds(display, &displayBounds);

  int x, y;
  if (width > displayBounds.w || height > displayBounds.h)
  {
    x = 0;
    y = 0;
  }
  else
  {
    x = displayBounds.x + ((displayBounds.w - width) / 2);
    y = displayBounds.y + ((displayBounds.h - height) / 2);
  }

  SDL_SetWindowPosition(g_mainWindow.window, x, y);
  SDL_SyncWindow(g_mainWindow.window);
}

//------------------------------------------------------------------------------
// Aplica o modo de exibição atual (g_isOriginalResolution) redimensionando a
// janela principal: tamanho nativo da imagem ou 1024x768 fixo. A imagem em
// si é sempre desenhada esticada para preencher totalmente a janela
// principal (ver renderizar_tela_princ()), então, quando a janela tem o mesmo
// tamanho da imagem, o resultado é a resolução original "pixel a pixel".
//------------------------------------------------------------------------------
void apply_display_mode(void)
{
  int width, height;
  if (g_isOriginalResolution)
  {
    width = (int)g_currentImage->rect.w;
    height = (int)g_currentImage->rect.h;
  }
  else
  {
    width = MAIN_WINDOW_WIDTH;
    height = MAIN_WINDOW_HEIGHT;
  }

  reposition_main_window(width, height);
}

//------------------------------------------------------------------------------
// Ação do botão "Equalizar" / "Ver original": alterna g_currentImage entre a
// imagem original em escala de cinza e a imagem equalizada (já calculada
// antecipadamente), sem precisar recarregar o arquivo de imagem.
//------------------------------------------------------------------------------
void toggle_equalize(void)
{
  g_isEqualized = !g_isEqualized;
  g_currentImage = g_isEqualized ? &g_imageEqualized : &g_imageGrayscale;

  atualizar_histograma();
  layout_secondary_window_widgets();
}

//------------------------------------------------------------------------------
// Ação do botão "Resolução original" / "1024x768": alterna o modo de
// exibição e redimensiona/reposiciona a janela principal de acordo.
//------------------------------------------------------------------------------
void toggle_resolution(void)
{
  g_isOriginalResolution = !g_isOriginalResolution;
  apply_display_mode();
  layout_secondary_window_widgets();
}

//------------------------------------------------------------------------------
// Salva a imagem atualmente exibida (g_currentImage) em OUTPUT_IMAGE_FILENAME
// (output_image.png), sobrescrevendo se já existir. Sempre imprime uma
// mensagem no terminal indicando o resultado (item 7 do enunciado).
//------------------------------------------------------------------------------
void salvar_imagem(void)
{
  SDL_Log(">>> salvar_imagem()");

  // Verifica se o arquivo já existe, só para poder informar corretamente se
  // ele foi criado ou sobrescrito.
  bool fileAlreadyExists = false;
  FILE *existingFile = fopen(OUTPUT_IMAGE_FILENAME, "rb");
  if (existingFile)
  {
    fileAlreadyExists = true;
    fclose(existingFile);
  }

  bool saved = IMG_SavePNG(g_currentImage->surface, OUTPUT_IMAGE_FILENAME);
  if (!saved)
  {
    SDL_Log("\t*** Erro ao salvar \"%s\": %s", OUTPUT_IMAGE_FILENAME, SDL_GetError());
  }
  else if (fileAlreadyExists)
  {
    SDL_Log("\tArquivo \"%s\" sobrescrito com sucesso.", OUTPUT_IMAGE_FILENAME);
  }
  else
  {
    SDL_Log("\tArquivo \"%s\" criado com sucesso.", OUTPUT_IMAGE_FILENAME);
  }

  SDL_Log("<<< salvar_imagem()");
}

//------------------------------------------------------------------------------
// Desenha a imagem atual (g_currentImage) esticada para preencher toda a
// janela principal.
//------------------------------------------------------------------------------
void renderizar_tela_princ(void)
{
  int windowWidth = 0, windowHeight = 0;
  SDL_GetWindowSize(g_mainWindow.window, &windowWidth, &windowHeight);

  SDL_SetRenderDrawColor(g_mainWindow.renderer, 0, 0, 0, COR_MAX);
  SDL_RenderClear(g_mainWindow.renderer);

  SDL_FRect destRect = { .x = 0.0f, .y = 0.0f, .w = (float)windowWidth, .h = (float)windowHeight };
  SDL_RenderTexture(g_mainWindow.renderer, g_currentImage->texture, NULL, &destRect);

  SDL_RenderPresent(g_mainWindow.renderer);
}

//------------------------------------------------------------------------------
// Desenha o conteúdo da janela secundária: título, histograma, estatísticas
// (média/desvio padrão + classificações) e os dois botões.
//------------------------------------------------------------------------------
void renderizar_tela_seg(void)
{
  SDL_SetRenderDrawColor(g_secondaryWindow.renderer, 30, 30, 30, COR_MAX);
  SDL_RenderClear(g_secondaryWindow.renderer);

  SDL_Color textColor = { 255, 255, 255, COR_MAX };

  renderizar_texto(g_secondaryWindow.renderer, "Histograma", (float)MARGIN, (float)MARGIN, textColor);

  renderizar_histograma(g_secondaryWindow.renderer, &g_histogramaArea);

  char mediaText[96];
  snprintf(mediaText, sizeof(mediaText), "Intensidade media: %.1f (%s)", g_histogramamedia, brilho_class(g_histogramamedia));
  renderizar_texto(g_secondaryWindow.renderer, mediaText, (float)MARGIN, g_statsLine1Y, textColor);

  char desvio_padraoText[96];
  snprintf(desvio_padraoText, sizeof(desvio_padraoText), "Desvio padrao: %.1f (contraste %s)", g_histogramadesvio_padrao, contraste_class(g_histogramadesvio_padrao));
  renderizar_texto(g_secondaryWindow.renderer, desvio_padraoText, (float)MARGIN, g_statsLine2Y, textColor);

  renderizar_botao(g_secondaryWindow.renderer, &g_buttonEqualize);
  renderizar_botao(g_secondaryWindow.renderer, &g_buttonResolution);

  SDL_RenderPresent(g_secondaryWindow.renderer);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static SDL_AppResult initialize(void)
{
  SDL_Log(">>> initialize()");

  SDL_Log("\tIniciando SDL...");
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    SDL_Log("\t*** Erro ao iniciar a SDL: %s", SDL_GetError());
    SDL_Log("<<< initialize()");
    return SDL_APP_FAILURE;
  }

  SDL_Log("\tIniciando SDL_ttf...");
  if (!TTF_Init())
  {
    SDL_Log("\t*** Erro ao iniciar a SDL_ttf: %s", SDL_GetError());
    SDL_Log("<<< initialize()");
    return SDL_APP_FAILURE;
  }

  SDL_Log("\tCarregando fonte \"%s\"...", FONT_FILENAME);
  g_font = TTF_OpenFont(FONT_FILENAME, FONT_SIZE);
  if (!g_font)
  {
    SDL_Log("\t*** Erro ao carregar a fonte \"%s\": %s", FONT_FILENAME, SDL_GetError());
    SDL_Log("\t*** Verifique se o arquivo .ttf está na mesma pasta do executável (ver README.md).");
    SDL_Log("<<< initialize()");
    return SDL_APP_FAILURE;
  }

  SDL_Log("\tCriando janela principal...");
  if (!MyWindow_initialize(&g_mainWindow, MAIN_WINDOW_TITLE, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, 0))
  {
    SDL_Log("\t*** Erro ao criar a janela principal: %s", SDL_GetError());
    SDL_Log("<<< initialize()");
    return SDL_APP_FAILURE;
  }

  SDL_Log("\tCriando janela secundária...");
  if (!MyWindow_initialize(&g_secondaryWindow, SECONDARY_WINDOW_TITLE, SECONDARY_WINDOW_WIDTH, SECONDARY_WINDOW_HEIGHT, 0))
  {
    SDL_Log("\t*** Erro ao criar a janela secundária: %s", SDL_GetError());
    SDL_Log("<<< initialize()");
    return SDL_APP_FAILURE;
  }

  // Torna a janela secundária "filha" da janela principal (item 3 do
  // enunciado). Disponível em versões recentes da SDL3; caso a função não
  // exista na versão instalada, atualize a biblioteca ou consulte a
  // documentação da SDL3 pela alternativa equivalente.
  SDL_SetWindowParent(g_secondaryWindow.window, g_mainWindow.window);
  SDL_SetWindowPosition(g_secondaryWindow.window, 0, 0);

  // Posição/tamanho inicial da janela principal: modo 1024x768, centralizada.
  reposition_main_window(MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT);

  SDL_Log("<<< initialize()");
  return SDL_APP_CONTINUE;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static void shutdown(void)
{
  SDL_Log(">>> shutdown()");

  MyImage_destroy(&g_imageGrayscale);
  MyImage_destroy(&g_imageEqualized);

  if (g_font)
  {
    SDL_Log("\tFechando fonte...");
    TTF_CloseFont(g_font);
    g_font = NULL;
  }

  MyWindow_destroy(&g_secondaryWindow);
  MyWindow_destroy(&g_mainWindow);

  SDL_Log("\tEncerrando SDL_ttf...");
  TTF_Quit();

  SDL_Log("\tEncerrando SDL...");
  SDL_Quit();

  SDL_Log("<<< shutdown()");
}

//------------------------------------------------------------------------------
// Carrega a imagem passada por linha de comando, detecta se é colorida ou
// escala de cinza, converte se necessário e pré-calcula a versão equalizada
// (itens 1 e 2 do enunciado).
//------------------------------------------------------------------------------
static bool load_and_prepare_image(const char *imagePath)
{
  if (!load_rgba32(imagePath, g_mainWindow.renderer, &g_imageGrayscale))
  {
    // load_rgba32() já imprime a mensagem de erro detalhada.
    return false;
  }

  if (escala_cinza(g_imageGrayscale.surface))
  {
    SDL_Log("Imagem de entrada: escala de cinza.");
  }
  else
  {
    SDL_Log("Imagem de entrada: colorida. Convertendo para escala de cinza...");
    converte_cinza(g_imageGrayscale.surface);

    // A textura foi criada a partir dos pixels originais (coloridos); agora
    // que a superfície foi alterada, é preciso recriar a textura.
    SDL_DestroyTexture(g_imageGrayscale.texture);
    g_imageGrayscale.texture = SDL_CreateTextureFromSurface(g_mainWindow.renderer, g_imageGrayscale.surface);
    if (!g_imageGrayscale.texture)
    {
      SDL_Log("\t*** Erro ao recriar textura após conversão para escala de cinza: %s", SDL_GetError());
      return false;
    }
  }

  // Pré-calcula a versão equalizada, para que o botão "Equalizar" apenas
  // troque qual imagem é exibida (sem recalcular nem recarregar nada).
  if (!equalizar_imagem(g_mainWindow.renderer, &g_imageGrayscale, &g_imageEqualized))
  {
    SDL_Log("\t*** Erro ao pré-calcular a equalização da imagem.");
    return false;
  }

  g_currentImage = &g_imageGrayscale;
  atualizar_histograma();

  return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static void loop(void)
{
  SDL_Log(">>> loop()");

  renderizar_tela_princ();
  renderizar_tela_seg();

  SDL_Event event;
  bool isRunning = true;
  while (isRunning)
  {
    bool mustRefreshMain = false;
    bool mustRefreshSecondary = false;

    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_EVENT_QUIT:
        isRunning = false;
        break;

      case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_S && !event.key.repeat)
        {
          salvar_imagem();
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        if (event.motion.windowID == SDL_GetWindowID(g_secondaryWindow.window))
        {
          ButtonState previousEqualizeState = g_buttonEqualize.state;
          ButtonState previousResolutionState = g_buttonResolution.state;

          // Só atualiza para HOVER se o botão não estiver com o mouse
          // pressionado (o estado PRESSED é tratado nos eventos de clique).
          if (g_buttonEqualize.state != BUTTON_STATE_PRESSED)
          {
            g_buttonEqualize.state = point_in_rect(event.motion.x, event.motion.y, &g_buttonEqualize.rect)
              ? BUTTON_STATE_HOVER : BUTTON_STATE_NEUTRAL;
          }
          if (g_buttonResolution.state != BUTTON_STATE_PRESSED)
          {
            g_buttonResolution.state = point_in_rect(event.motion.x, event.motion.y, &g_buttonResolution.rect)
              ? BUTTON_STATE_HOVER : BUTTON_STATE_NEUTRAL;
          }

          if (g_buttonEqualize.state != previousEqualizeState || g_buttonResolution.state != previousResolutionState)
          {
            mustRefreshSecondary = true;
          }
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT && event.button.windowID == SDL_GetWindowID(g_secondaryWindow.window))
        {
          if (point_in_rect(event.button.x, event.button.y, &g_buttonEqualize.rect))
          {
            g_buttonEqualize.state = BUTTON_STATE_PRESSED;
            mustRefreshSecondary = true;
          }
          if (point_in_rect(event.button.x, event.button.y, &g_buttonResolution.rect))
          {
            g_buttonResolution.state = BUTTON_STATE_PRESSED;
            mustRefreshSecondary = true;
          }
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT && event.button.windowID == SDL_GetWindowID(g_secondaryWindow.window))
        {
          // Só confirma o clique se o mouse ainda estiver em cima do botão
          // no momento em que foi solto (padrão de UI: permite cancelar o
          // clique arrastando o mouse para fora antes de soltar).
          if (g_buttonEqualize.state == BUTTON_STATE_PRESSED)
          {
            bool stillInside = point_in_rect(event.button.x, event.button.y, &g_buttonEqualize.rect);
            g_buttonEqualize.state = stillInside ? BUTTON_STATE_HOVER : BUTTON_STATE_NEUTRAL;
            if (stillInside)
            {
              toggle_equalize();
              mustRefreshMain = true;
            }
            mustRefreshSecondary = true;
          }

          if (g_buttonResolution.state == BUTTON_STATE_PRESSED)
          {
            bool stillInside = point_in_rect(event.button.x, event.button.y, &g_buttonResolution.rect);
            g_buttonResolution.state = stillInside ? BUTTON_STATE_HOVER : BUTTON_STATE_NEUTRAL;
            if (stillInside)
            {
              toggle_resolution();
              mustRefreshMain = true;
            }
            mustRefreshSecondary = true;
          }
        }
        break;
      }
    }

    if (mustRefreshMain)
    {
      renderizar_tela_princ();
    }
    if (mustRefreshSecondary)
    {
      renderizar_tela_seg();
    }

    // Pequena pausa para não consumir 100% da CPU enquanto nada muda.
    SDL_Delay(10);
  }

  SDL_Log("<<< loop()");
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  atexit(shutdown);

  if (argc != 2)
  {
    SDL_Log("*** Uso: %s caminho_da_imagem.ext", argv[0]);
    return SDL_APP_FAILURE;
  }

  if (initialize() == SDL_APP_FAILURE)
  {
    return SDL_APP_FAILURE;
  }

  if (!load_and_prepare_image(argv[1]))
  {
    return SDL_APP_FAILURE;
  }

  layout_secondary_window_widgets();
  apply_display_mode();

  loop();

  return 0;
}