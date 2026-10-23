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
			"sus sonidos, cada uno después de su nombre y de lo que significa.",
			"El radar suena en cuatro direcciones cuando cambia lo que te rodea; abajo, más grave. Ahora te reproduzco "
			"sus sonidos, cada uno después de su nombre y de lo que significa."},
		{"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca: personas, lugares y "
			"lo que hay en el suelo, los objetos con los sonidos que elegiste en la lista de ajustes. Hasta ahora lo "
			"tenía callado para que oyeras el radar solo. Te reproduzco cada sonido después de su nombre.",
			"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca: personas, lugares y "
			"lo que hay en el suelo, los objetos con los sonidos que elegiste en la lista de ajustes. Hasta ahora lo "
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
		{"Fuera del campamento hay monstruos, y cada clase suena a su manera desde donde está. Antes de salir, te los "
			"reproduzco, cada uno después de su nombre.",
			"Fuera del campamento hay monstruos, y cada clase suena a su manera desde donde está. Antes de salir, te los "
			"reproduzco, cada uno después de su nombre."},
		{"Estás en la zona {moor}. Al aire libre, dos consultas te sitúan: {view} y {b} dan un resumen del terreno, y "
			"{view} y {rs} hacia arriba, la descripción del área. Prueba el resumen.",
			"Estás en la zona {moor}. Control más O te da un resumen del terreno que te rodea. Pruébalo."},
		{"Aquí hay monstruos: suenan como los oíste en el campamento, y el escáner los lista en su propia categoría. "
			"Ataca con {a}; tus otras habilidades van en los botones que les diste. El doble pitido de la mira dice que tu "
			"ataque apunta a un monstruo: ataca cuando lo oigas, y si calla, gira hacia el gruñido hasta que pite, para "
			"no gastar ningún ataque en una pared o un barril. Si la vida baja, bebe una poción con "
			"{dpad}. Lo que suelten suena desde donde cae: acércate y pulsa {a} para recogerlo; el oro se recoge al pasar "
			"por encima. Mata algún monstruo.",
			"Aquí hay monstruos: suenan como los oíste en el campamento, y el escáner los lista en su propia categoría. "
			"Tabulador elige el monstruo siguiente; mantén A para atacar con tu habilidad izquierda, y W con la derecha. "
			"El doble pitido de la mira dice que tu ataque apunta a un monstruo: ataca cuando lo oigas, para no gastar "
			"ningún ataque en una pared o un barril. Si la vida baja, las pociones van en las teclas del 1 al 4. Lo que suelten suena desde donde cae: Punto elige "
			"el objeto del suelo siguiente, Coma el anterior, y D lo recoge; el oro se recoge al pasar por encima. Mata "
			"algún monstruo."},
		{"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Busca ahí {den}: en cuanto aparezca, la ruta y la baliza te llevan "
			"a su entrada. Si te pierdes, la categoría zona sin explorar del escáner te dice adónde ir.",
			"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Busca ahí {den}: en cuanto aparezca, la ruta y la baliza te llevan "
			"a su entrada. Si te pierdes, la categoría zona sin explorar del escáner te dice adónde ir."},
		{"Estás en {den}. En cuevas y mazmorras el mod conoce las salas: {view} y {y} dicen las salidas de la sala "
			"donde estás. Pruébalo ahora.",
			"Estás en {den}. En cuevas y mazmorras el mod conoce las salas: Control más Y dice las salidas de la sala "
			"donde estás. Pruébalo ahora."},
		{"Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los monstruos de {den}; el juego "
			"dice cuántos quedan. La categoría zona sin explorar del escáner te ayuda a no dejarte ninguno.",
			"Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los monstruos de {den}; el juego "
			"dice cuántos quedan. La categoría zona sin explorar del escáner te ayuda a no dejarte ninguno."},
		{"Ya no quedan monstruos en {den}. Vuelve al campamento: abre el inventario, usa el pergamino de portal y entra "
			"en el portal con {a}. También puedes volver andando con la ruta y la baliza.",
			"Ya no quedan monstruos en {den}. Vuelve al campamento: abre el inventario con I, ve al pergamino de portal y "
			"pulsa D "
			"para leerlo; cierra el inventario y pulsa D junto al portal. También puedes volver andando con la ruta y "
			"la baliza."},
		{"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"{menu} y gástalo.",
			"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"T y gástalo."},
		{"Una cosa más que el juego no te cuenta: los puntos de viaje guardan tu camino. El juego guarda las misiones "
			"que has hecho, pero cada vez que vuelves empiezas en el campamento, y a una zona lejana se llega por su punto "
			"de viaje, que solo es tuyo cuando lo has tocado. El primero fuera del campamento está en {plains}, pasada la "
			"zona {moor}. Búscalo en la categoría de puntos de viaje del escáner, acércate y pulsa {a}: se abre su menú, "
			"y ya es tuyo. Cierra el menú con {b}.",
			"Una cosa más que el juego no te cuenta: los puntos de viaje guardan tu camino. El juego guarda las misiones "
			"que has hecho, pero cada vez que vuelves empiezas en el campamento, y a una zona lejana se llega por su punto "
			"de viaje, que solo es tuyo cuando lo has tocado. El primero fuera del campamento está en {plains}, pasada la "
			"zona {moor}. Búscalo en la categoría de puntos de viaje del escáner, acércate y pulsa D: se abre su menú, "
			"y ya es tuyo. Cierra el menú con Escape."},
		{"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la mayoría de los menús, {rs} "
			"hacia abajo te dice qué hacen allí los botones: pruébalo en cada menú nuevo para ti, como el alijo. En la "
			"lista de ajustes, con {view} y {rb}, eliges qué dice y qué suena el mod. Desde aquí, el juego es tuyo.",
			"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la lista de ajustes, con "
			"Control más S, eliges qué dice y qué suena el mod. El teclado tiene además atajos que caminan por ti: H va "
			"a la zona sin explorar más cercana, y Mayúsculas más Inicio a lo que nombró el escáner. "
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
		"Zona sin explorar", "Oro en el suelo", "Una poción en el suelo",
		"Un objeto en el suelo que te puedes poner, como un arma"},
	.worldTask = {
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes.",
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes."},
	.enemyCues = {"Monstruo: los más cercanos gruñen desde donde están", "Monstruo que ataca de lejos, como un arquero",
		"Chamán que devuelve la vida a los monstruos muertos: mátalo primero", "Élite: un campeón, un monstruo único o un jefe",
		"A tu alcance: un monstruo ha entrado en el alcance de tu ataque",
		"Mira: tu ataque apunta a un monstruo que ves"},
	.leaveTask = {
		"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
		"ruta, sigue la baliza y cruza la salida.",
		"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
		"ruta, sigue la baliza y cruza la salida."},
	.auditionAgain = {
		"Van uno tras otro; no tienes que pulsar nada. Para oír un sonido otra vez, pulsa {view} y {rs} a la izquierda: "
		"el siguiente espera entonces hasta {view} y {rs} a la derecha. Más tarde, al recorrer estos mensajes hacia "
		"atrás, cada sonido vuelve a sonar.",
		"Van uno tras otro; no tienes que pulsar nada. Para oír un sonido otra vez, pulsa K: el siguiente espera "
		"entonces hasta L. Más tarde, al recorrer estos mensajes hacia atrás, cada sonido vuelve a sonar."},
	.auditionWait = {
		"Después de cada sonido, {view} y {rs} a la izquierda lo repiten, y {view} y {rs} a la derecha pasan al "
		"siguiente. Más tarde, al recorrer estos mensajes hacia atrás, cada sonido vuelve a sonar.",
		"Después de cada sonido, K lo repite, y L pasa al siguiente. Más tarde, al recorrer estos mensajes hacia "
		"atrás, cada sonido vuelve a sonar."},
	.portalButton = {
		"Más rápido: {portal} abre un portal al momento, sin pasar por el inventario, mientras lleves un pergamino o "
		"un tomo de portal.",
		"Más rápido: {portal} abre un portal al momento, sin pasar por el inventario, mientras lleves un pergamino o "
		"un tomo de portal."},
	.gold = {
		"Has recogido oro; se recoge al pasar por encima. Al morir pierdes parte del oro que llevas encima, pero el del "
		"alijo está a salvo. De vuelta en el campamento, abre el alijo y deposita el que no necesites.",
		"Has recogido oro; se recoge al pasar por encima. Al morir pierdes parte del oro que llevas encima, pero el del "
		"alijo está a salvo. De vuelta en el campamento, abre el alijo y deposita el que no necesites."},
	.shop = {
		"Una tienda. Si tienes el oro, cómprale a Akara un {idtome} y un {portaltome}: cada uno guarda hasta 20 "
		"pergaminos en un hueco de una columna de ancho y dos filas de alto, y los pergaminos de su clase que compres después van dentro. Los pergaminos "
		"sueltos ocupan una casilla cada uno y enseguida llenan el inventario. Un pergamino de identificación muestra "
		"qué es un objeto sin identificar: úsalo y elige el objeto.",
		"Una tienda. Si tienes el oro, cómprale a Akara un {idtome} y un {portaltome}: cada uno guarda hasta 20 "
		"pergaminos en un hueco de una columna de ancho y dos filas de alto, y los pergaminos de su clase que compres después van dentro. Los pergaminos "
		"sueltos ocupan una casilla cada uno y enseguida llenan el inventario. Un pergamino de identificación muestra "
		"qué es un objeto sin identificar: úsalo y elige el objeto."},
	.levelUp = {
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa {menu} para abrir "
		"tu ficha; te guío dentro.",
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa C para abrir tu "
		"ficha; te guío dentro."},
	.levelUpSheet = {
		"Tu ficha. Estás en el más de Fuerza: con {dpad} arriba y abajo pasas por los cuatro atributos, y manteniendo "
		"{a} gastas un punto en el que estés; manteniendo {x}, cinco de golpe. Con {rs} arriba y abajo lees tus estadísticas línea a línea, y a la "
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
		"botón en el que la quieras. La lectura de cada habilidad dice en qué botón está. Las habilidades pasivas, como "
		"las maestrías, actúan solas: no necesitan botón ni pueden tenerlo. Con LT y RT, pasadas las ramas de tu "
		"clase, la página {general} tiene lo que tiene todo personaje, como el portal y la identificación; los pones en "
		"botones del mismo modo.",
		"Para usar una habilidad, ponla en una mano: S abre la lista de habilidades de la mano derecha, y Mayúsculas "
		"más S la de la izquierda; las flechas eligen una y A la pone en esa mano. Con una lista abierta, F1 a F8 "
		"asignan a esa tecla la habilidad que tiene el cursor. Las habilidades pasivas, como las maestrías, actúan "
		"solas: no están en estas listas y no necesitan nada más."},
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
		"Hecho. Prueba ahora tu nueva habilidad una vez: pulsa el botón en el que la pusiste y escucha cómo suena. Cada "
		"vez que subas de nivel, {menu} abre tu ficha. Volvemos a lo que estabas haciendo:",
		"Hecho. Prueba ahora tu nueva habilidad una vez: A usa la habilidad de tu mano izquierda y W la de la derecha; "
		"escucha cómo suena. Cada vez que subas de nivel, C abre tu ficha y T el árbol de habilidades. Volvemos a lo "
		"que estabas haciendo:"},
	.routeStick = "{view} y {rs} hacia abajo",
	.routeButton = "{view} y {a}",
	.firstMessage = "Primer mensaje",
	.lastMessage = "Último mensaje",
	// Spain's names (game-data/local/lng/strings/levels.json, item-names.json, ui.json); Mexico's differ.
	.den = "Cubil del Mal",
	.moor = "Landa de Sangre",
	.plains = "Llanuras Frías",
	.portalTome = "Tomo del portal de la ciudad",
	.identifyTome = "Tomo de identificación",
	.generalSkills = "Habilidades generales",
};
}
