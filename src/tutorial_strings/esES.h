#pragma once
#include "tutorial_text.h"

// Spanish, Spain's and Mexico's (esES): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts esES{
	.welcome = {
		"Tutorial de D2R Access. Te enseñaré a orientarte con el mod hasta completar la primera misión. "
		"Cada paso te pide algo; al hacerlo sonará un aviso de paso completado y llegará el siguiente. "
		"{view} y {rs} a la izquierda o a la derecha recorren estos mensajes; {view} y {up} vuelven al primero. "
		"Puedes apagar el tutorial en la lista de ajustes, con {view} y {rb}.",
		"Tutorial de D2R Access. Te enseñaré a orientarte con el mod hasta completar la primera misión. "
		"Cada paso te pide algo; al hacerlo sonará un aviso de paso completado y llegará el siguiente. "
		"K y L recorren estos mensajes; Mayúsculas más K vuelve al primero. "
		"Puedes apagar el tutorial en la lista de ajustes, con Control más S."},
	.steps = {
		{"Primer paso: tu estado. Pulsa {view} y {x} para oír tu vida y tu maná.",
			"Primer paso: tu estado. Pulsa Z para oír tu vida, y X para tu maná."},
		{"El radar suena en cuatro direcciones cuando cambia lo que te rodea; abajo, más grave. Ahora te reproduzco "
			"sus sonidos, cada uno después de su nombre y de lo que significa; no tienes que pulsar nada.",
			"El radar suena en cuatro direcciones cuando cambia lo que te rodea; abajo, más grave. Ahora te reproduzco "
			"sus sonidos, cada uno después de su nombre y de lo que significa; no tienes que pulsar nada."},
		{"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca. Hasta ahora lo "
			"tenía callado para que oyeras el radar solo. Te reproduzco cada sonido después de su nombre.",
			"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca. Hasta ahora lo "
			"tenía callado para que oyeras el radar solo. Te reproduzco cada sonido después de su nombre."},
		{"El escáner. Lista lo que hay en el mapa por categorías: salidas, PNJ, puntos de viaje y más, del más "
			"cercano al más lejano, con su distancia y dirección. Con {rs}, arriba y abajo cambian de categoría, e "
			"izquierda y derecha la recorren. Busca a Warriv en la categoría PNJ: es el que tienes más cerca.",
			"El escáner. Lista lo que hay en el mapa por categorías: salidas, PNJ, puntos de viaje y más, del más "
			"cercano al más lejano, con su distancia y dirección. Control más Re Pág y Control más Av Pág cambian de "
			"categoría; Re Pág y Av Pág la recorren. Busca a Warriv en la categoría PNJ: es el que tienes más cerca."},
		{"Habla con Warriv. Está a pocos pasos: acércate con {ls} hacia donde dijo el escáner, pulsa {a} a su lado y "
			"escucha lo que tiene que decirte. Cuando termine, cierra el diálogo con {b}.",
			"Habla con Warriv. Está a pocos pasos: acércate con las flechas hacia donde dijo el escáner, pulsa D a su "
			"lado y escucha lo que tiene que decirte. Cuando termine, cierra el diálogo con Escape."},
		{"Ahora Akara, la sanadora del campamento: tiene la primera misión. Está más lejos; búscala en la categoría "
			"PNJ del escáner.",
			"Ahora Akara, la sanadora del campamento: tiene la primera misión. Está más lejos; búscala en la categoría "
			"PNJ del escáner."},
		{"La ruta. Te dice el camino hasta lo que nombró el escáner, en direcciones y distancias, como se planea con "
			"el minimapa; no camina por ti. Con Akara nombrada, pulsa {route}.",
			"La ruta. Te dice el camino hasta lo que nombró el escáner, en direcciones y distancias, como se planea con "
			"el minimapa; no camina por ti. Con Akara nombrada, pulsa Control más E."},
		{"La baliza. Pita hacia lo que nombró el escáner y te guía por el camino, pero andas tú. Pulsa {view} y {lb} "
			"para seguir a Akara y camina hacia los pitidos hasta llegar a ella; otra pulsación la suelta.",
			"La baliza. Pita hacia lo que nombró el escáner y te guía por el camino, pero andas tú. Pulsa Control más B "
			"para seguir a Akara y camina con las flechas hacia los pitidos hasta llegar a ella; otra pulsación la suelta."},
		{"Habla con Akara: a su lado, pulsa {a}. Tiene una misión para ti. "
			"Cuando termine, cierra el diálogo con {b}.",
			"Habla con Akara: a su lado, pulsa D. Tiene una misión para ti. "
			"Cuando termine, cierra el diálogo con Escape."},
		{"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
			"ruta, sigue la baliza y cruza la salida.",
			"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
			"ruta, sigue la baliza y cruza la salida."},
		{"Estás en la zona {moor}. Tres consultas te sitúan: {view} y {y} dicen las salidas de la zona donde "
			"estás; {view} y {b}, un resumen del terreno; {view} y {rs} hacia arriba, la descripción del área. "
			"Prueba las dos primeras.",
			"Estás en la zona {moor}. Dos consultas te sitúan: Control más Y dice las salidas de la zona donde "
			"estás, y Control más O, un resumen del terreno. Prueba las dos."},
		{"Aquí hay monstruos. Gruñen al acercarse, y el escáner los lista en su propia categoría. Ataca con {a}; tus "
			"otras habilidades van en los botones que les diste. Si la vida baja, bebe una poción con {dpad}. Mata algún "
			"monstruo y recoge el oro que suelte: acércate y pulsa {a}.",
			"Aquí hay monstruos. Gruñen al acercarse, y el escáner los lista en su propia categoría. Tabulador elige el "
			"monstruo siguiente; mantén A para atacar con tu habilidad izquierda, y W con la derecha. Si la vida baja, "
			"las pociones van en las teclas del 1 al 4. Mata algún monstruo y recoge su oro: Punto elige el objeto del "
			"suelo siguiente y Coma el anterior, y D lo recoge."},
		{"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Si te pierdes, la categoría sala inexplorada del escáner te dice "
			"adónde ir. Entra en la cueva.",
			"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Si te pierdes, la categoría sala inexplorada del escáner te dice "
			"adónde ir. Entra en la cueva."},
		{"Estás en {den}. Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los "
			"monstruos de la cueva; el juego dice cuántos quedan. La categoría sala inexplorada del escáner te ayuda a "
			"no dejarte ninguno.",
			"Estás en {den}. Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los "
			"monstruos de la cueva; el juego dice cuántos quedan. La categoría sala inexplorada del escáner te ayuda a "
			"no dejarte ninguno."},
		{"La cueva está limpia. Vuelve al campamento: abre el inventario, usa el pergamino de portal y entra en el "
			"portal con {a}. También puedes volver andando con la ruta y la baliza.",
			"La cueva está limpia. Vuelve al campamento: abre el inventario con I, ve al pergamino de portal y pulsa D "
			"para leerlo; cierra el inventario y pulsa D junto al portal. También puedes volver andando con la ruta y "
			"la baliza."},
		{"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"{menu} y gástalo.",
			"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"T y gástalo."},
		{"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la lista de ajustes, con {view} "
			"y {rb}, eliges qué dice y qué suena el mod. Desde aquí, el juego es tuyo.",
			"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la lista de ajustes, con "
			"Control más S, eliges qué dice y qué suena el mod. El teclado tiene además atajos que caminan por ti: H va "
			"a la sala sin explorar más cercana, y Mayúsculas más Inicio a lo que nombró el escáner. "
			"Desde aquí, el juego es tuyo."}},
	.radarTask = {
		"Ahora camina con {ls} hasta oír una pared.",
		"Ahora camina con las flechas hasta oír una pared."},
	.cues = {"Pasaje: un hueco que lleva a otra sala o a una salida",
		"Pasaje sin explorar: el pasaje con dos pitidos al final; lleva a terreno que aún no has visto",
		"Suelo: por ahí puedes andar; más adelante puede haber de todo, también un pasaje",
		"Suelo sin explorar: el suelo con los dos pitidos; por ahí hay terreno que aún no has visto", "Pared: te corta el paso",
		"Puerta: se abre para pasar", "Obstáculo pequeño: algo pequeño que te corta el paso y que puedes rodear"},
	.worldCues = {"PNJ, alguien con quien hablar", "PNJ con una exclamación: tiene algo para ti",
		"Alijo, el cofre donde guardas tus objetos", "Escaleras, entrada o portal", "Borde de zona, el paso a otra zona",
		"Sala sin explorar"},
	.worldTask = {
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes.",
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes."},
	.levelUp = {
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa {menu} para abrir "
		"tu ficha; te guío dentro.",
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa C para abrir tu "
		"ficha; te guío dentro."},
	.levelUpSheet = {
		"Tu ficha. Estás en el más de Fuerza: con {dpad} arriba y abajo pasas por los cuatro atributos, y manteniendo "
		"{a} gastas un punto en el que estés. Con {rs} arriba y abajo lees tus estadísticas línea a línea, y a la "
		"derecha, los puntos que te quedan.",
		"Tu ficha. Las flechas llevan el cursor al más de cada atributo, y A gasta un punto en él. J lee la "
		"descripción de la línea en la que estás, y Mayúsculas con flecha arriba o abajo la recorre línea a línea."},
	.levelUpSwitch = {
		"Ya has repartido los atributos. Ahora tu punto de habilidad: pulsa {rb} hasta llegar al árbol de "
		"habilidades; cada menú dice su nombre.",
		"Ya has repartido los atributos. Ahora tu punto de habilidad: pulsa T para abrir el árbol de habilidades junto "
		"a tu ficha."},
	.levelUpTree = {
		"El árbol de habilidades. LT y RT cambian de rama, {dpad} recorre sus habilidades y {rs} arriba lee la que "
		"tienes. {a} gasta el punto en ella, y {rs} a la derecha dice los puntos que te quedan. Cuando acabes, "
		"ciérralo con {b}.",
		"El árbol de habilidades. Re Pág y Av Pág cambian de rama, las flechas recorren sus habilidades y J lee la "
		"que tiene el cursor. A gasta el punto en ella, y Control más Av Pág dice los puntos que te quedan. Cuando "
		"acabes, ciérralo con Escape."},
	.levelUpTreeLater = {
		"Aún te queda el punto de habilidad. Pulsa {menu}: con los atributos repartidos, abre el árbol de habilidades.",
		"Aún te queda el punto de habilidad. Pulsa T para abrir el árbol de habilidades."},
	.levelUpBind = {
		"Para usar una habilidad, ponla en un botón: en el árbol de habilidades, sobre ella, pulsa {x} y después el "
		"botón en el que la quieras. La lectura de cada habilidad dice en qué botón está.",
		"Para usar una habilidad, ponla en una mano: S abre la lista de habilidades de la mano derecha, y Mayúsculas "
		"más S la de la izquierda; las flechas eligen una y A la pone en esa mano. Con una lista abierta, F1 a F8 "
		"asignan a esa tecla la habilidad que tiene el cursor."},
	.kit = {
		"Has recogido algo que te puedes poner. Pulsa {menu} para abrir los menús; se abren en tu inventario, y te "
		"guío dentro.",
		"Has recogido algo que te puedes poner. Pulsa I para abrir tu inventario; te guío dentro."},
	.kitOpen = {
		"Tu inventario. Con {dpad} vas de casilla en casilla, por tu mochila y por lo que llevas puesto, y el mod dice "
		"qué hay en cada una. En un objeto, {rs} arriba lee su descripción, y {rs} abajo, los botones que puedes usar "
		"con él, entre ellos el de equiparlo. Un objeto que dice sin identificar no se puede equipar hasta "
		"identificarlo, con un pergamino de identificación, que vende Akara.",
		"Tu inventario. Las flechas llevan el cursor de casilla en casilla, por tu mochila y por lo que llevas puesto, "
		"y el mod dice qué hay en cada una. J lee la descripción de un objeto, y Mayúsculas más A lo equipa. Un objeto "
		"que dice sin identificar no se puede equipar hasta identificarlo, con un pergamino de identificación, que "
		"vende Akara."},
	.kitWorn = {
		"Ya lo llevas puesto. Cierra el inventario con {b} cuando acabes.",
		"Ya lo llevas puesto. Cierra el inventario con Escape cuando acabes."},
	.kitDone = {
		"Hecho. Lo que recojas, en el inventario te lo pones. Volvemos a lo que estabas haciendo:",
		"Hecho. Lo que recojas, en el inventario te lo pones. Volvemos a lo que estabas haciendo:"},
	.levelUpDone = {
		"Hecho. Cada vez que subas de nivel, {menu} abre tu ficha. Volvemos a lo que estabas haciendo:",
		"Hecho. Cada vez que subas de nivel, C abre tu ficha y T el árbol de habilidades. Volvemos a lo que estabas "
		"haciendo:"},
	.routeStick = "{view} y {rs} hacia abajo",
	.routeButton = "{view} y {a}",
	.firstMessage = "Primer mensaje",
	.lastMessage = "Último mensaje",
	// Spain's names (game-data/local/lng/strings/levels.json); Mexico's are Guarida del Mal and Páramo Sangriento.
	.den = "Cubil del Mal",
	.moor = "Landa de Sangre",
};
}
