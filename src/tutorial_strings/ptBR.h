#pragma once
#include "tutorial_text.h"

// Brazilian Portuguese (ptBR): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts ptBR{
	.welcome = {
		"Tutorial do D2R Access. Vou mostrar como você se orienta com o mod até concluir a primeira missão. "
		"Cada passo pede que você faça algo; quando você faz, toca o som de passo concluído e vem o próximo. "
		"{view} e {rs} para a esquerda ou para a direita percorrem estas mensagens; {view} e {up} voltam à primeira. "
		"Você pode desligar o tutorial na lista de ajustes, com {view} e {rb}.",
		"Tutorial do D2R Access. Vou mostrar como você se orienta com o mod até concluir a primeira missão. "
		"Cada passo pede que você faça algo; quando você faz, toca o som de passo concluído e vem o próximo. "
		"K e L percorrem estas mensagens; Shift mais K volta à primeira. "
		"Você pode desligar o tutorial na lista de ajustes, com Ctrl mais S."},
	.steps = {
		{"Primeiro passo: o seu estado. Pressione {view} e {x} para ouvir a sua vida e a sua mana.",
			"Primeiro passo: o seu estado. Pressione Z para ouvir a sua vida, e X para a sua mana."},
		{"O radar soa em quatro direções conforme o que está ao redor muda; para baixo, soa mais grave. Agora vou tocar "
			"os sons dele, cada um depois do seu nome e do que significa; você não precisa pressionar nada.",
			"O radar soa em quatro direções conforme o que está ao redor muda; para baixo, soa mais grave. Agora vou tocar "
			"os sons dele, cada um depois do seu nome e do que significa; você não precisa pressionar nada."},
		{"Além do radar, as coisas por perto soam de onde estão, mais alto quanto mais perto. Até agora eu os mantive "
			"baixos para você ouvir só o radar. Vou tocar cada um depois do seu nome.",
			"Além do radar, as coisas por perto soam de onde estão, mais alto quanto mais perto. Até agora eu os mantive "
			"baixos para você ouvir só o radar. Vou tocar cada um depois do seu nome."},
		{"O scanner. Ele lista o que há no mapa por categoria: saídas, NPCs, pontos de passagem e mais, do mais próximo, com "
			"a distância e a direção. Com {rs}, para cima e para baixo mudam a categoria, e esquerda e direita a percorrem. "
			"Encontre Warriv na categoria de NPCs: ele é o mais próximo de você.",
			"O scanner. Ele lista o que há no mapa por categoria: saídas, NPCs, pontos de passagem e mais, do mais próximo, com "
			"a distância e a direção. Ctrl mais Page Up e Ctrl mais Page Down mudam a categoria; Page Up e "
			"Page Down a percorrem. Encontre Warriv na categoria de NPCs: ele é o mais próximo de você."},
		{"Fale com Warriv. Ele está a poucos passos: ande até ele com {ls} como o scanner disse, pressione {a} ao lado "
			"dele e ouça o que ele tem a dizer. Quando ele terminar, feche o diálogo com {b}.",
			"Fale com Warriv. Ele está a poucos passos: ande até ele com as setas como o scanner disse, pressione "
			"D ao lado dele e ouça o que ele tem a dizer. Quando ele terminar, feche o diálogo com Esc."},
		{"Agora Akara, a curandeira do acampamento: ela tem a primeira missão. Ela está mais longe; encontre-a na categoria "
			"de NPCs do scanner.",
			"Agora Akara, a curandeira do acampamento: ela tem a primeira missão. Ela está mais longe; encontre-a na categoria "
			"de NPCs do scanner."},
		{"A rota. Ela diz o caminho até o que o scanner nomeou, em direções e distâncias, como você planejaria "
			"no minimapa; ela não anda por você. Com Akara nomeada, pressione {route}.",
			"A rota. Ela diz o caminho até o que o scanner nomeou, em direções e distâncias, como você planejaria "
			"no minimapa; ela não anda por você. Com Akara nomeada, pressione Ctrl mais E."},
		{"A guia. Ela emite sinais na direção do que o scanner nomeou e conduz você pelo caminho, "
			"mas quem anda é você. Pressione {view} e {lb} para seguir Akara, e ande em direção aos sinais até chegar nela; "
			"outro toque a solta.",
			"A guia. Ela emite sinais na direção do que o scanner nomeou e conduz você pelo caminho, "
			"mas quem anda é você. Pressione Ctrl mais B para seguir Akara, e ande com as setas em direção aos sinais até chegar nela; "
			"outro toque a solta."},
		{"Fale com Akara: ao lado dela, pressione {a}. Ela tem uma missão para você. "
			"Quando ela terminar, feche o diálogo com {b}.",
			"Fale com Akara: ao lado dela, pressione D. Ela tem uma missão para você. "
			"Quando ela terminar, feche o diálogo com Esc."},
		{"Agora saia do acampamento por conta própria, com o que aprendeu: entre as saídas do scanner, encontre {moor}, "
			"peça a rota, siga a guia e atravesse a saída.",
			"Agora saia do acampamento por conta própria, com o que aprendeu: entre as saídas do scanner, encontre {moor}, "
			"peça a rota, siga a guia e atravesse a saída."},
		{"Você está em {moor}. Três perguntas situam você: {view} e {y} dizem as saídas da área em que você está; "
			"{view} e {b}, uma visão geral do terreno; {view} e {rs} para cima, a descrição da área. Experimente as duas primeiras.",
			"Você está em {moor}. Duas perguntas situam você: Ctrl mais Y diz as saídas da área em que você está, "
			"e Ctrl mais O, uma visão geral do terreno. Experimente as duas."},
		{"Há monstros aqui. Eles rosnam quando se aproximam, e o scanner os lista em uma categoria própria. "
			"Ataque com {a}; as suas outras habilidades estão nos botões que você deu a elas. Se a sua vida cair, beba uma poção "
			"com {dpad}. Mate um monstro e pegue o ouro que ele deixa: chegue perto e pressione {a}.",
			"Há monstros aqui. Eles rosnam quando se aproximam, e o scanner os lista em uma categoria própria. "
			"Tab escolhe o próximo monstro; segure A para atacar com a habilidade da esquerda, e W para a da direita. Se a sua vida "
			"cair, as poções estão nas teclas 1 a 4. Mate um monstro e pegue o ouro dele: ponto escolhe o próximo item "
			"no chão e vírgula o anterior, e D o pega."},
		{"Encontre {den}. O mapa é refeito a cada jogo, então a entrada pode estar perto ou longe. Siga os sons do "
			"radar, ou os tons das paredes se você os ligou: passagens inexploradas e chão inexplorado levam a terreno novo, e o "
			"radar avisa quando registra uma nova saída, que o scanner então lista entre as suas saídas. Se você se perder, "
			"a categoria sala inexplorada do scanner mostra para onde ir. Entre na caverna.",
			"Encontre {den}. O mapa é refeito a cada jogo, então a entrada pode estar perto ou longe. Siga os sons do "
			"radar, ou os tons das paredes se você os ligou: passagens inexploradas e chão inexplorado levam a terreno novo, e o "
			"radar avisa quando registra uma nova saída, que o scanner então lista entre as suas saídas. Se você se perder, "
			"a categoria sala inexplorada do scanner mostra para onde ir. Entre na caverna."},
		{"Você está em {den}. Ao entrar em cada sala, você ouvirá se partes dela ainda estão inexploradas. Mate todos os "
			"monstros da caverna; o jogo diz quantos restam. A categoria sala inexplorada do scanner ajuda você a não deixar nenhum para trás.",
			"Você está em {den}. Ao entrar em cada sala, você ouvirá se partes dela ainda estão inexploradas. Mate todos os "
			"monstros da caverna; o jogo diz quantos restam. A categoria sala inexplorada do scanner ajuda você a não deixar nenhum para trás."},
		{"A caverna está limpa. Volte ao acampamento: abra o inventário, use o pergaminho do Portal da Cidade e entre no "
			"portal com {a}. Você também pode voltar andando com a rota e a guia.",
			"A caverna está limpa. Volte ao acampamento: abra o inventário com I, vá até o pergaminho do Portal da Cidade e pressione D "
			"para lê-lo; feche o inventário e pressione D ao lado do portal. Você também pode voltar andando com a rota e "
			"a guia."},
		{"Fale com Akara para receber a sua recompensa, um ponto de habilidade. Depois abra a árvore de habilidades com {menu} e gaste-o.",
			"Fale com Akara para receber a sua recompensa, um ponto de habilidade. Depois abra a árvore de habilidades com T e gaste-o."},
		{"Tutorial concluído. Você sabe se orientar: scanner, rota, guia e radar. Na lista de ajustes, "
			"com {view} e {rb}, você escolhe o que o mod diz e toca. A partir daqui, o jogo é seu.",
			"Tutorial concluído. Você sabe se orientar: scanner, rota, guia e radar. Na lista de ajustes, "
			"com Ctrl mais S, você escolhe o que o mod diz e toca. O teclado também tem atalhos que andam "
			"por você: H vai até a sala inexplorada mais próxima, e Shift mais Home até o que o scanner nomeou. "
			"A partir daqui, o jogo é seu."}},
	.radarTask = {
		"Agora ande com {ls} até ouvir uma parede.",
		"Agora ande com as setas até ouvir uma parede."},
	.cues = {"Passagem: uma abertura que leva a outra sala ou a uma saída",
		"Passagem inexplorada: a passagem com dois bipes no final; ela leva a terreno que você ainda não viu",
		"Chão livre: você pode andar nessa direção; mais adiante pode haver qualquer coisa, até uma passagem",
		"Chão inexplorado: o chão com os dois bipes; nessa direção há terreno que você ainda não viu", "Parede: ela bloqueia o seu caminho",
		"Porta: ela abre para deixar você passar", "Obstáculo pequeno: algo pequeno no seu caminho que você pode contornar"},
	.worldCues = {"NPC, alguém para conversar", "NPC com ponto de exclamação: ele tem algo para você",
		"Baú pessoal, o baú onde você guarda os seus itens", "Escada, entrada ou portal", "Limite da área, o caminho para outra área",
		"Sala inexplorada"},
	.worldTask = {
		"Agora vão soar os que estão ao seu redor. Se você não sabe o que um som é, o scanner diz o que há ali, "
		"e cada som pode ser desligado na lista de ajustes.",
		"Agora vão soar os que estão ao seu redor. Se você não sabe o que um som é, o scanner diz o que há ali, "
		"e cada som pode ser desligado na lista de ajustes."},
	.levelUp = {
		"Você subiu de nível: tem pontos de atributos e um ponto de habilidade para gastar. Pressione {menu} para abrir a "
		"tela do personagem; vou guiar você lá dentro.",
		"Você subiu de nível: tem pontos de atributos e um ponto de habilidade para gastar. Pressione C para abrir a "
		"tela do personagem; vou guiar você lá dentro."},
	.levelUpSheet = {
		"A tela do personagem. Você está no mais de Força: {dpad} para cima e para baixo percorrem os quatro atributos, e "
		"manter {a} pressionado gasta um ponto no atributo em que você está. {rs} para cima e para baixo leem as suas estatísticas linha por linha, e "
		"para a direita diz os pontos que restam.",
		"A tela do personagem. As setas levam o cursor ao mais de cada atributo, e A gasta um ponto nele. "
		"J lê a descrição da linha em que você está, e Shift com a seta para cima ou para baixo percorre linha por "
		"linha."},
	.levelUpSwitch = {
		"Os seus pontos de atributos foram gastos. Agora o seu ponto de habilidade: pressione {rb} até chegar à árvore de habilidades; cada menu "
		"diz o seu nome.",
		"Os seus pontos de atributos foram gastos. Agora o seu ponto de habilidade: pressione T para abrir a árvore de habilidades ao lado da tela do personagem."},
	.levelUpTree = {
		"A árvore de habilidades. LT e RT mudam o ramo, {dpad} percorre as habilidades dele, e {rs} para cima lê a habilidade em que você "
		"está. {a} gasta o ponto nela, e {rs} para a direita diz os pontos que restam. Quando terminar, feche-a "
		"com {b}.",
		"A árvore de habilidades. Page Up e Page Down mudam o ramo, as setas percorrem as habilidades dele, e J lê "
		"a habilidade em que o cursor está. A gasta o ponto nela, e Ctrl mais Page Down diz os pontos que "
		"restam. Quando terminar, feche-a com Esc."},
	.levelUpTreeLater = {
		"Você ainda tem o seu ponto de habilidade. Pressione {menu}: com os atributos gastos, ele abre a árvore de habilidades.",
		"Você ainda tem o seu ponto de habilidade. Pressione T para abrir a árvore de habilidades."},
	.levelUpBind = {
		"Para usar uma habilidade, coloque-a em um botão: na árvore de habilidades, sobre a habilidade, pressione {x} e depois o botão "
		"que quiser. A leitura de cada habilidade diz em qual botão ela está.",
		"Para usar uma habilidade, coloque-a em uma mão: S abre a lista de habilidades da mão direita, e Shift mais S a "
		"da mão esquerda; as setas escolhem uma e A a coloca nessa mão. Com uma lista aberta, F1 a F8 associam a habilidade "
		"sob o cursor a essa tecla."},
	.kit = {
		"Você pegou algo para vestir. Pressione {menu} para abrir os menus; eles abrem no seu inventário, e vou "
		"guiar você lá dentro.",
		"Você pegou algo para vestir. Pressione I para abrir o seu inventário; vou guiar você lá dentro."},
	.kitOpen = {
		"O seu inventário. {dpad} vai de célula em célula, pela mochila e pelo que você está vestindo, e o mod diz "
		"o que cada uma contém. Em um item, {rs} para cima lê a descrição dele, e {rs} para baixo os botões que você pode usar com ele, "
		"entre eles o que o equipa. Um item que diz não identificado não pode ser vestido até ser identificado, "
		"com um pergaminho de Identificação, que Akara vende.",
		"O seu inventário. As setas levam o cursor de célula em célula, pela mochila e pelo que você está "
		"vestindo, e o mod diz o que cada uma contém. J lê a descrição de um item, e Shift mais A o equipa. Um item "
		"que diz não identificado não pode ser vestido até ser identificado, com um pergaminho de Identificação, que Akara vende."},
	.kitWorn = {
		"Você o está vestindo. Feche o inventário com {b} quando terminar.",
		"Você o está vestindo. Feche o inventário com Esc quando terminar."},
	.kitDone = {
		"Pronto. O que quer que você pegue, o inventário é onde você veste. De volta ao que você estava fazendo:",
		"Pronto. O que quer que você pegue, o inventário é onde você veste. De volta ao que você estava fazendo:"},
	.levelUpDone = {
		"Pronto. Sempre que você subir de nível, {menu} abre a tela do personagem. De volta ao que você estava fazendo:",
		"Pronto. Sempre que você subir de nível, C abre a tela do personagem e T a árvore de habilidades. De volta ao que você estava "
		"fazendo:"},
	.routeStick = "{view} e {rs} para baixo",
	.routeButton = "{view} e {a}",
	.firstMessage = "Primeira mensagem",
	.lastMessage = "Última mensagem",
	.den = "Covil do Mal",
	.moor = "Charco Sangrento",
};
}
